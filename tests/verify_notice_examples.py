import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
doc = (root / 'docs/message-notification.md').read_text(encoding='utf-8')
snippet = re.search(r'```cpp\n(.*?)```', doc, re.S).group(1)
compiled = (root / 'tests/support/notice_example.hpp').read_text(encoding='utf-8')
assert re.sub(r'\s+', '', snippet) == re.sub(r'\s+', '', compiled), 'notice docs example differs from compiled fixture'
print('notice documentation example matches compiled and event-tested fixture')
