import sys
import subprocess as sp


file1 = sys.argv[1]
file2 = sys.argv[2]

res = sp.run(["git", "diff", "-u", "--no-index", file1, file2], capture_output=True)

res_text = res.stdout.decode()

only_added = []

counter = 0
for line in res_text.splitlines():
    if counter < 4:
        counter = counter + 1
        continue

    if line[0] == "+":
        only_added.append(line[1:])

for line in only_added:
    print(line)
