# Traceroute Implementation in Rust

A simple traceroute utility written in Rust using the `pnet` crate. This program sends ICMP Echo Request packets with increasing Time-To-Live (TTL) values to map the network path to a specified destination IP address.

## Features

- **ICMP Packet Handling**: Sends ICMP Echo Request packets and listens for responses.
- **TTL Management**: Increments the TTL value to trace each hop to the destination.
- **Round-Trip Time Measurement**: Calculates the time taken for each packet to reach the hop and return.
- **Destination Detection**: Stops tracing once the destination is reached.

## Prerequisites

- **Rust**: Ensure you have Rust and Cargo installed. You can install Rust using [rustup](https://rustup.rs/).
- **Administrator Privileges**: Root or administrator rights are required to send raw ICMP packets.

## Installation

1. **Clone the Repository**

   ```bash
   git clone https://github.com/junfengyu/traceroute_rust.git
   cd traceroute_rust
   ```

2. **Update Dependencies**

   Ensure that your `Cargo.toml` includes the necessary dependencies:

   ```toml
   [dependencies]
   pnet = "0.28.0"
   pnet_packet = "0.28.0"
   pnet_transport = "0.28.0"
   ```

   Then run:

   ```bash
   cargo update
   ```

3. **Build the Project**

   Build the release version for optimal performance:

   ```bash
   cargo build --release
   ```


### Run the Program**

Run the program with root privileges, specifying the destination as a command-line argument:

- **Linux/macOS**:

  ```bash
  sudo ./target/release/traceroute-rust example.com
  ```

- **Windows**:

  Open the Command Prompt or PowerShell as Administrator and run:

  ```cmd
  .\target\release\traceroute-rust.exe 8.8.8.8
  ```

## Example Output

```
Traceroute to 8.8.8.8
1    192.168.1.1      1.23ms
2    10.0.0.1         5.67ms
3    172.16.0.1       10.45ms
4    8.8.8.8          15.78ms
Destination reached.
```



## License

This project is licensed under the [MIT License](LICENSE).

