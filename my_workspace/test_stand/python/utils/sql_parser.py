"""
sql_parser.py

Разбивает текст с несколькими SQL-командами на отдельные statements
и группирует их в блоки:
  - {"type": "statement", "sql": "..."}                — одиночный запрос вне транзакции
  - {"type": "transaction", "statements": [...], "outcome": "commit"|"rollback"|"unterminated"}
    — явный блок BEGIN ... COMMIT/ROLLBACK (весь список включая сами BEGIN/COMMIT)

Корректно учитывает:
  - строки в одинарных и двойных кавычках ('...', "..."), включая экранирование ''
  - dollar-quoted строки $$...$$ и $tag$...$tag$ (тела функций/процедур)
  - строчные комментарии -- ...
  - блочные комментарии /* ... */
  - ';' внутри всего перечисленного не считается разделителем

Использование:
    from sql_parser import parse_sql_file, parse_sql_text

    blocks = parse_sql_file("my_queries.sql")
    for block in blocks:
        if block["type"] == "transaction":
            run_transaction(block["statements"])
        else:
            run_single_statement(block["sql"])
"""

import re
from typing import List, Dict, Any

BEGIN_RE = re.compile(r'^(BEGIN|START\s+TRANSACTION)\b', re.IGNORECASE)
COMMIT_RE = re.compile(r'^(COMMIT|END)\b', re.IGNORECASE)
ROLLBACK_RE = re.compile(r'^ROLLBACK\b', re.IGNORECASE)
DOLLAR_TAG_RE = re.compile(r'\$([A-Za-z_][A-Za-z0-9_]*)?\$')


def split_sql_statements(sql_text: str) -> List[str]:
    """
    Разбивает сырой SQL-текст на список отдельных statements по символу ';',
    игнорируя ';' внутри строковых литералов, dollar-quoted блоков и комментариев.
    """
    statements = []
    current = []
    i = 0
    n = len(sql_text)

    in_single = False
    in_double = False
    in_line_comment = False
    in_block_comment = False
    dollar_tag = None  # None -> не внутри $$; иначе хранит тег (может быть '')

    while i < n:
        ch = sql_text[i]

        if in_line_comment:
            current.append(ch)
            if ch == '\n':
                in_line_comment = False
            i += 1
            continue

        if in_block_comment:
            current.append(ch)
            if ch == '*' and i + 1 < n and sql_text[i + 1] == '/':
                current.append('/')
                i += 2
                in_block_comment = False
                continue
            i += 1
            continue

        if dollar_tag is not None:
            closing = f'${dollar_tag}$'
            if sql_text.startswith(closing, i):
                current.append(closing)
                i += len(closing)
                dollar_tag = None
                continue
            current.append(ch)
            i += 1
            continue

        if in_single:
            current.append(ch)
            if ch == "'":
                if i + 1 < n and sql_text[i + 1] == "'":
                    current.append("'")
                    i += 2
                    continue
                in_single = False
            i += 1
            continue

        if in_double:
            current.append(ch)
            if ch == '"':
                in_double = False
            i += 1
            continue

        if ch == '-' and i + 1 < n and sql_text[i + 1] == '-':
            in_line_comment = True
            current.append(ch)
            i += 1
            continue

        if ch == '/' and i + 1 < n and sql_text[i + 1] == '*':
            in_block_comment = True
            current.append(ch)
            i += 1
            continue

        if ch == "'":
            in_single = True
            current.append(ch)
            i += 1
            continue

        if ch == '"':
            in_double = True
            current.append(ch)
            i += 1
            continue

        if ch == '$':
            m = DOLLAR_TAG_RE.match(sql_text, i)
            if m:
                dollar_tag = m.group(1) or ''
                current.append(m.group(0))
                i = m.end()
                continue

        if ch == ';':
            stmt = ''.join(current).strip()
            if stmt:
                statements.append(stmt)
            current = []
            i += 1
            continue

        current.append(ch)
        i += 1

    tail = ''.join(current).strip()
    if tail:
        statements.append(tail)

    return statements


def _strip_leading_comments(stmt: str) -> str:
    s = stmt
    changed = True
    while changed:
        changed = False
        s2 = s.lstrip()
        if s2.startswith('--'):
            nl = s2.find('\n')
            s = s2[nl + 1:] if nl != -1 else ''
            changed = True
            continue
        if s2.startswith('/*'):
            end = s2.find('*/')
            s = s2[end + 2:] if end != -1 else ''
            changed = True
            continue
        if s2 != s:
            s = s2
    return s.lstrip()


def group_into_blocks(statements: List[str]) -> List[Dict[str, Any]]:
    blocks = []
    current_txn = None

    for raw in statements:
        core = _strip_leading_comments(raw)
        if not core:
            continue

        if BEGIN_RE.match(core):
            if current_txn is not None:
                current_txn.setdefault("outcome", "unterminated")
                blocks.append(current_txn)
            current_txn = {"type": "transaction", "statements": [raw]}
            continue

        if COMMIT_RE.match(core) or ROLLBACK_RE.match(core):
            if current_txn is not None:
                current_txn["statements"].append(raw)
                current_txn["outcome"] = "commit" if COMMIT_RE.match(core) else "rollback"
                blocks.append(current_txn)
                current_txn = None
            else:
                blocks.append({"type": "statement", "sql": raw})
            continue

        if current_txn is not None:
            current_txn["statements"].append(raw)
        else:
            blocks.append({"type": "statement", "sql": raw})

    if current_txn is not None:
        current_txn.setdefault("outcome", "unterminated")
        blocks.append(current_txn)

    return blocks


def parse_sql_text(sql_text: str) -> List[Dict[str, Any]]:
    statements = split_sql_statements(sql_text)
    return group_into_blocks(statements)


def parse_sql_file(path: str) -> List[Dict[str, Any]]:
    with open(path, "r", encoding="utf-8") as f:
        return parse_sql_text(f.read())


if __name__ == "__main__":
    import json
    import sys

    if len(sys.argv) != 2:
        print("Использование: python sql_parser.py <path_to.sql>")
        sys.exit(1)

    result = parse_sql_file(sys.argv[1])
    print(json.dumps(result, indent=2, ensure_ascii=False))
