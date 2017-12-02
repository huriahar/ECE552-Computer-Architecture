import re

with open("out.txt", 'r') as fh:
    num_sum = 0
    for line in fh:
        line = line.strip()
        if line.startswith("openend: MISPRED_PER_1K_INST"):
            m = re.search(":\s+(\d+.\d+)$", line)
            print(m.group(1))
            num_sum += float(m.group(1))
    print ("Average mispredictions/1000 instructions: " + str(num_sum/8))