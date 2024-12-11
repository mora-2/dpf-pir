import secrets

def generate_random_string(length):
    return secrets.token_hex(length // 2)

random_string = generate_random_string(16)
print(random_string)
