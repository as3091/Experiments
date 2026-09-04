import pikepdf
def compute_task(item):
    try:
        # Attempt to decrypt
        with pikepdf.open(filename_or_stream = "check.pdf", password=item):
            print(f"\n Success! Password found: {item}")
            return item
    except pikepdf.PasswordError:
        # continue  # Wrong password, keep trying
        # print(f"\n {item} is not the correct password.")
        return None
    except Exception as e:
        print(f"\n⚠️ Unexpected error: {e}")
        return None