import os
Import("env")

env_file_path = os.path.join(env["PROJECT_DIR"], ".env")

if os.path.isfile(env_file_path):
    with open(env_file_path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            key, value = line.split("=", 1)
            env.Append(CPPDEFINES=[(key.strip(), env.StringifyMacro(value.strip()))])