import socket

# Define host and port
HOST = "127.0.0.1"  # The server's hostname or IP address (localhost)
PORT = 8080  # The port used by the server

basePath = ".././data/scheduling-benchmarks/wtsds/"
instanceName = "wt_sds_2.instance"
instancePath = basePath + instanceName


# Create a socket object (AF_INET = IPv4, SOCK_STREAM = TCP)
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:

    # with open(instancePath, 'r') as file:
    #     instanceData = file.read()
    try:
        # Connect to the server
        s.connect((HOST, PORT))

        # Send some data
        s.sendall(b"Hello, Server!")

        # Receive a response
        data = s.recv(1024)
        print(f"Received from server: {data.decode()}")

    except ConnectionRefusedError:
        print("Error: Could not connect. Is the server running on port 8080?")
