import socket
import struct
import time
import csv

numdata = 100

csv_file = "data.csv"   # <-- your CSV filename

# === Load price data from CSV ===
data_values = []

with open(csv_file, "r") as f:
    reader = csv.DictReader(f)

    for row in reader:
        price_str = row["Price"]          # e.g. "26,202.95"
        price_str = price_str.replace(",", "")  # remove thousands separator → "26202.95"

        # Convert to integer (drop decimals)
        price_int = int(float(price_str))

        data_values.append(price_int)

print("Loaded", len(data_values), "values:", data_values)


# === TCP client setup ===
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("192.168.1.10", 7))

index = 0
print("Starting transmission...")


# Build a packet of 100 consecutive values
packet = []
for _ in range(numdata):
    packet.append(data_values[index])
    index = (index + 1) % len(data_values)  # wrap around
    



# Pack as 100 unsigned ints
data = struct.pack('I' * numdata, *packet)
print("Sending:", packet)
s.sendall(data)

time.sleep(1)
s.close()
