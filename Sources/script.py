import serial # library for serial communication (UART)
import time   # library for delays and timing

# ==========================================
# 1. Configuration & Connection
# ==========================================
try: 
    # Initialize the Serial Connection
    # 'COM3': the physical port my STM32 is connected to (can check in device manager)
    # 115200: Baud rate (Speed). Must match the STM32 code exactly
    # timeout = 1: Safety Feature. If we try to read and nothing comes,
    #              give up after 1 second. Prevents the program from freezing forever
    ser = serial.Serial('COM3', 115200, timeout = 1)
    print(f"Successfully connected to {ser.portstr}")
except Exception as e:
    # if the cable is unplugged or COM port is busy, print the error and stop.
    print(f"Error connecting to serial port: {e}")
    exit()

# ==========================================
# 2. Automated Handshake (Startup Check)
# ==========================================
print("Pinging STM32...")

system_online = False

# [Critical Step] -> "clear the mailbox"
# when hardware powers up, it might send garbage bytes due to hardware reasons.
# we flush the buffer to ensure we are listening to fresh data.
ser.reset_input_buffer() # serial library function to clear the buffer

# [Retry] mechanism: try 3 times in case the first message gets lost.
for i in range(3):
    ser.write(b'H') # send 'H' (Handshake request) to STM32
    print(f"Ping attemp {i + 1}...")

    # Read the response
    # .decode(): Convert computer bytes to human text
    # .strip(): Remove invisible newline characters (\r\n)
    response = ser.readline().decode('utf-8', errors='ignore').strip()

    # If STM32 replies with the keyword "Ready"
    if "Ready" in response: # STM32 says it is ready
        print(f"[STM32]: {response}")
        system_online = True
        break # Handshake is successful, break out of the re-try loop

# If the loop finished and we still haven't heard back...
if system_online == False: # Handshake Failed
    print("Error: STM32 did not respond. Is it plugged in?")
    ser.close()
    exit()

print("Connection Verified! Entering Control Loop.")

# ==========================================
# 3. Main Control Loop
# ==========================================
try:
    while True:
        # Prompt user for input.
        # .upper() handles 'f' and 'F' automatically.
        user_input = input("Enter command (F to feed cat, H to ping, Q to quit): ").upper()

        # Reset Buffer before using ser.write(b...) to send any command to STM32
        # this clears any culmulated "garbage messages" or "history messages"
        ser.reset_input_buffer()

        # --- QUIT LOGIC ---
        if user_input == 'Q':
            print("Exiting program...")
            break # use break instead of exit() so it will execute ser.close() below to release resources 
        
        # --- FEED LOGIC (Blocking) ---
        elif user_input == 'F':
            ser.write(b'F') # send byte 'F'
            print(f"[PC] Sent: F (Feeding...)")

            print("Waiting here for confirmation from STM32...")

            # Start a timer to prevent deadlocks
            start_time = time.time()

            # Keep checking for response for up to 5 seconds
            while (time.time() -  start_time) < 5: 
                response = ser.readline().decode('utf-8', errors='ignore').strip()
                if response:
                    print(f"[STM32]: {response}")
                    if "Complete" in response:
                        break
            else:
                # this executes only if the 5 seconds ran out (Timeout)
                print("[Warning] No response received from STM32 (Timeout)")

        # --- PING LOGIC (Non-Blocking / Fast Check) ---
        elif user_input == 'H':
            print("[PC] Sent: H (Ping)")
            ser.write(b'H') # handshake

            # [FIXED BUG HERE]
            # Give the electrical signal time to travel and STM32 to process.
            # Without this sleep, Python checks too fast and sees an empty buffer
            time.sleep(0.1)

            if ser.in_waiting: # Check: Is there data in the buffer?
                response = ser.readline().decode('utf-8', errors='ignore').strip()
                print(f"[STM32]: {response}")
            else:
                print("[Warning] No response. Connection might be lost.")
        # --- INVALID INPUT ---
        else:
            print("Invalid Command. Please enter 'F', 'H', or 'Q'.")

# ==========================================
# 4. Safe Shutdown
# ==========================================
# Handles errors gracefully
except KeyboardInterrupt:
    # Handles the Ctrl+C event
    print("\nProgram terminated by Ctrl + C.")

except serial.SerialException as e:
    # this will catch the exception when I unplug STM32 during middle of this Python script
    print(f"\n[Error] Connection lost! The device seems to be disconnected: {e}")

except Exception as e:
    # some errors other than the two already covered
    print(f"\n[Error] An unexpected error occurred: {e}")

finally:
    # Best Practice: Always close the resource.
    # This runs whether the program ends by 'Q', Ctrl+C, or an error.
    if ser.is_open:
        ser.close()
    print("Serial connection closes safely.")
