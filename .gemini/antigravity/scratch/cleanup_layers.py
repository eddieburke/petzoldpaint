
import sys

with open(r'c:\Users\Eddie\Desktop\Custom Programs\peztoldpaint\layers.c', 'rb') as f:
    lines = f.readlines()

# Lines in view_file are 1-indexed.
# We want to remove what was lines 607 to 618 (approx)
# Actually, let's just find the block and remove it.

start_idx = -1
end_idx = -1

for i, line in enumerate(lines):
    # Match the beginning of the orphaned block
    if b'const char *oldName =' in line and i > 600 and i < 620:
        start_idx = i - 2 # including the empty lines
    if b'HistoryPushFormatted("Blend Mode:' in line:
        # The next line should be the closing brace
        if i + 1 < len(lines) and b'}' in lines[i+1]:
            end_idx = i + 1

if start_idx != -1 and end_idx != -1:
    del lines[start_idx:end_idx+1]
    with open(r'c:\Users\Eddie\Desktop\Custom Programs\peztoldpaint\layers.c', 'wb') as f:
        f.writelines(lines)
    print(f"Removed lines {start_idx+1} to {end_idx+1}")
else:
    print(f"Could not find block. Start: {start_idx}, End: {end_idx}")
