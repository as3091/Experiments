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
            value = value.strip()
            # Strip surrounding quotes if present (handles quoted strings for spaces/special chars)
            if (value.startswith('"') and value.endswith('"')) or (value.startswith("'") and value.endswith("'")):
                value = value[1:-1]
            # Define which keys should be treated as integers (add more as needed)
            int_keys = {"ON_BUTTON_PIN", "OFF_BUTTON_PIN", "LED_PIN","BUTTON_PIN"}
            if key in int_keys:
                # Treat as int (will error if not numeric)
                env.Append(CPPDEFINES=[(key, int(value))])
            else:
                # Stringify everything else (safe for numeric strings in SSID/pass)
                env.Append(CPPDEFINES=[(key, env.StringifyMacro(value))])