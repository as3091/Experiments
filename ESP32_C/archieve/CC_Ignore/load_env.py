Import('env')
import os

def load_dotenv():
    env_path = os.path.join(env['PROJECT_DIR'], '.env')
    if not os.path.exists(env_path):
        print("Warning: .env file not found")
        return

    with open(env_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if '=' not in line:
                continue
            key, _, raw = line.partition('=')
            key = key.strip()
            raw = raw.strip()
            # Strip inline comments
            if ' #' in raw:
                raw = raw[:raw.index(' #')].strip()
            # Quoted string → C string literal
            if (raw.startswith('"') and raw.endswith('"')) or \
               (raw.startswith("'") and raw.endswith("'")):
                value = raw[1:-1]
                env.Append(CPPDEFINES=[(key, '\\"' + value + '\\"')])
            else:
                # Numeric or unquoted
                env.Append(CPPDEFINES=[(key, raw)])

load_dotenv()
