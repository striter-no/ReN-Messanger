

server - S1
client 1 - C1
client 2 - C2

target:
C1 -> text -> C2

steps:

C1 -> S1
C2 -> S1
    1. RSA pubkey transive
    2. Establishment of RSA connect
    3. Transiveing msgs to AES

C1 -> S1 -> C2
C2 -> S1 -> C1
    1. Transiveing new key for AES
    2. Loop:
        C1 <-msgs-> C2

        where messages are encrypted with AES

C1 -> AES no. 2 (f.C2) -> AES no. 1 (f.S1) -> S1
S1 -> looking AES no. 1 -> transv. AES no. 2 -> C2