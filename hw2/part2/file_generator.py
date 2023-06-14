import os

file_size = 1024 * 1024  # 1MB in bytes

dst_path = 'src-directory'
for i in range(1, 101):
    file_name = os.path.join(dst_path,f"file_{i}.txt")
    with open(file_name, "wb") as file:
        file.write(os.urandom(file_size))
    print(f"Created {file_name}")
