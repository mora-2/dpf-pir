import random
import json
import secrets

vowels = ['a', 'e', 'i', 'o', 'u']
consonants = ['b', 'c', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm', 'n', 'p', 'q', 'r', 's', 't', 'v', 'w', 'x', 'y', 'z']

def generate_name():
    length = random.randint(4, 8)
    name = ''
    for i in range(length):
        if i % 2 == 0:
            name += random.choice(consonants)
        else:
            name += random.choice(vowels)
    return name.capitalize()

def generate_random_string(length):
    return secrets.token_hex(length // 2)

def gen_json(size, length_et):
    names = set()
    numbers = []
    name_number_map = {}

    # Generate unique names
    while len(names) < size:
        name = generate_name()
        if name not in names:
            names.add(name)

    # Generate unique numbers
    while len(numbers) < size:
        number = generate_random_string(16)
        numbers.append(number)

    # Create name-number mapping
    for name, number in zip(names, numbers):
        name_number_map[name] = str(number)
    
    return name_number_map

# Save as JSON
with open('random_data_16.json', 'w') as file:
    json.dump(gen_json(1000000), file)
# # Save as JSON
# with open('names_and_numbers_250.json', 'w') as file:
#     json.dump(gen_json(2500000), file)
# # Save as JSON
# with open('names_and_numbers_500.json', 'w') as file:
#     json.dump(gen_json(5000000), file)
# # Save as JSON
# with open('names_and_numbers_750.json', 'w') as file:
#     json.dump(gen_json(7500000), file)
# # Save as JSON
# with open('names_and_numbers_1000.json', 'w') as file:
#     json.dump(gen_json(10000000), file)

print("名字和对应的数字已保存在 names_and_numbers.json 文件中。")