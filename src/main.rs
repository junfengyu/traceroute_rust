use pnet_packet::icmp::{echo_request::MutableEchoRequestPacket, IcmpPacket, IcmpTypes};
use pnet_packet::ip::IpNextHeaderProtocols;
use pnet_packet::{icmp, Packet};
use pnet_transport::{icmp_packet_iter, transport_channel, TransportChannelType::Layer3};
use std::env;
use std::net::IpAddr;
use std::time::{Duration, Instant};

const MAX_TTL: u8 = 30;
const TIMEOUT: Duration = Duration::from_secs(3);

fn main() {
    // Collect command-line arguments
    let args: Vec<String> = env::args().collect();
    let destination = if args.len() > 1 {
        &args[1]
    } else {
        eprintln!("Usage: {} <destination>", args[0]);
        std::process::exit(1);
    };

    let dest_ip: IpAddr = match destination.parse() {
        Ok(ip) => ip,
        Err(_) => {
            // Try to resolve the hostname to an IP address
            match dns_lookup::lookup_host(destination) {
                Ok(mut ips) => {
                    if let Some(ip) = ips.pop() {
                        ip
                    } else {
                        eprintln!("Could not resolve hostname: {}", destination);
                        std::process::exit(1);
                    }
                }
                Err(_) => {
                    eprintln!("Invalid IP address or hostname: {}", destination);
                    std::process::exit(1);
                }
            }
        }
    };

    println!("Traceroute to {}", dest_ip);

    for ttl in 1..=MAX_TTL {
        let start = Instant::now();
        match send_echo_request(dest_ip, ttl) {
            Ok(ip) => {
                let duration = start.elapsed();
                println!("{} \t{} \t{:.2?}", ttl, ip, duration);
                if ip == dest_ip {
                    println!("Destination reached.");
                    break;
                }
            }
            Err(e) => {
                println!("{} \t* \t{}", ttl, e);
            }
        }
    }
}

fn send_echo_request(dest: IpAddr, ttl: u8) -> Result<IpAddr, &'static str> {
    let protocol = Layer3(IpNextHeaderProtocols::Icmp);
    let (mut tx, mut rx) = transport_channel(1024, protocol).map_err(|_| "Failed to open channel")?;

    tx.set_ttl(ttl).map_err(|_| "Failed to set TTL")?;

    let mut buffer = [0u8; 64];
    let mut packet = MutableEchoRequestPacket::new(&mut buffer).ok_or("Failed to create packet")?;

    // Set the packet fields
    packet.set_icmp_type(IcmpTypes::EchoRequest);
    packet.set_sequence_number(1);
    packet.set_identifier(0);

    // Create an ICMP packet for checksum calculation
    let icmp_packet = IcmpPacket::new(packet.packet()).ok_or("Failed to create ICMP packet")?;
    let checksum = icmp::checksum(&icmp_packet);
    packet.set_checksum(checksum);

    tx.send_to(packet, dest).map_err(|_| "Failed to send packet")?;

    let mut iter = icmp_packet_iter(&mut rx);

    let start_time = Instant::now();
    loop {
        if start_time.elapsed() > TIMEOUT {
            return Err("Request timed out");
        }

        if let Ok((packet, addr)) = iter.next() {
            match packet.get_icmp_type() {
                IcmpTypes::TimeExceeded | IcmpTypes::EchoReply => {
                    return Ok(addr);
                }
                _ => continue,
            }
        }
    }
}
