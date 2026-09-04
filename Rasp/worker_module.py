import pikepdf
from typing import Any

def compute_task(items):
    try:
        # Attempt to decrypt
        with pikepdf.open(filename_or_stream = "check.pdf", password=item):
            print(f"\n Success! Password found: {item}")
            with open(f"{item}.txt", "a") as file:
                file.write(f"{item}\n")
            return item
    except pikepdf.PasswordError:
        # continue  # Wrong password, keep trying
        # print(f"\n {item} is not the correct password.")
        # return None
        [ass]
    except Exception as e:
        print(f"\n⚠️ Unexpected error: {e}")
        return None



def worker_task(item: tuple[int, str]) -> tuple[int, str, Any]:
    line_num, word = item
    result = compute_task(word)
    return line_num, word, result
