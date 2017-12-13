If this service is ever deployed for real, might be wise to test against a
test table rather than the real table, to avoid disturbing the real table.

Please follow ../README.md to start gdp-directoryd with an appropriate
db set up. Once installed, the gdc-test binary can be used (locally or
remotely) to test the gdp directory service (test cguid is hardwired
0xc1c1c1...):


./gdc-test add 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
./gdc-test add 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
./gdc-test find 2
./gdc-test find 3
./gdc-test find 15
./gdc-test find 16
./gdc-test remove 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
./gdc-test remove 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
./gdc-test find 2
./gdc-test find 3
./gdc-test find 15


Sample test run:

gdp-04[341] ./gdc-test add 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17
Error: extraneous parameter(s)
Usage: gdc-test { { add | remove } <eguid> <dguid>+ | find <dguid> }
gdp-04[342] ./gdc-test add 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
-> eguid [6b86b273ff34fce19d6b804eff5a3f5747ada4eaa22f1d49c01e52ddb7875b4b]
-> dguid [d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35]
oguid len 448
-> oguid [4e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce]
-> oguid [4b227777d4dd1fc61c6f884f48641d02b4d121d3fd328cb08b5531fcacdabf8a]
-> oguid [ef2d127de37b942baad06145e54b0c619a1f22327b2ebbcfbec78f5564afe39d]
-> oguid [e7f6c011776e8db7cd330b54174fd76f7d0216b612387a5ffcfb81e6f0919683]
-> oguid [7902699be42c8a8e46fbbb4501726517e86b22c56a189f7625a6da49081b2451]
-> oguid [2c624232cdd221771294dfbb310aca000a0df6ac8b66b696d90ef06fdefb64a3]
-> oguid [19581e27de7ced00ff1ce50b2047e7a567c76b1cbaebabe5ef03f7c3017bb5b7]
-> oguid [4a44dc15364204a80fe80e9039455cc1608281820fe2b24f1e5233ade6af1dd5]
-> oguid [4fc82b26aecb47d2868c4efbe3581732a3e7cbcc6c2efb32062c08170a05eeb8]
-> oguid [6b51d431df5d7f141cbececcf79edf3dd861c3b4069f0b11661a3eefacbba918]
-> oguid [3fdba35f04dc8c462986c992bcf875546257113072a909c162f7e470e581e278]
-> oguid [8527a891e224136950ff32ca212b45bc93f69fbb801c3b1ebedac52775f99e61]
-> oguid [e629fa6598d732768f7c726b4b621285f9c3b85303900aa912017db7617d8bdb]
-> oguid [b17ef6d19c7a5b1ee83b907c595526dcb1eb06db8227d650d5dda0a9f4ce8cd9]
Send len 516
...awaiting reply...
Recv len 516
<- dguid [d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35]
oguid len 448
<- oguid [4e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce]
<- oguid [4b227777d4dd1fc61c6f884f48641d02b4d121d3fd328cb08b5531fcacdabf8a]
<- oguid [ef2d127de37b942baad06145e54b0c619a1f22327b2ebbcfbec78f5564afe39d]
<- oguid [e7f6c011776e8db7cd330b54174fd76f7d0216b612387a5ffcfb81e6f0919683]
<- oguid [7902699be42c8a8e46fbbb4501726517e86b22c56a189f7625a6da49081b2451]
<- oguid [2c624232cdd221771294dfbb310aca000a0df6ac8b66b696d90ef06fdefb64a3]
<- oguid [19581e27de7ced00ff1ce50b2047e7a567c76b1cbaebabe5ef03f7c3017bb5b7]
<- oguid [4a44dc15364204a80fe80e9039455cc1608281820fe2b24f1e5233ade6af1dd5]
<- oguid [4fc82b26aecb47d2868c4efbe3581732a3e7cbcc6c2efb32062c08170a05eeb8]
<- oguid [6b51d431df5d7f141cbececcf79edf3dd861c3b4069f0b11661a3eefacbba918]
<- oguid [3fdba35f04dc8c462986c992bcf875546257113072a909c162f7e470e581e278]
<- oguid [8527a891e224136950ff32ca212b45bc93f69fbb801c3b1ebedac52775f99e61]
<- oguid [e629fa6598d732768f7c726b4b621285f9c3b85303900aa912017db7617d8bdb]
<- oguid [b17ef6d19c7a5b1ee83b907c595526dcb1eb06db8227d650d5dda0a9f4ce8cd9]
gdp-04[343] ./gdc-test find 2
-> dguid [d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35]
-> cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
...awaiting reply...
Recv len 100
<- dguid [d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35]
<- eguid [6b86b273ff34fce19d6b804eff5a3f5747ada4eaa22f1d49c01e52ddb7875b4b]
<- cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
gdp-04[344] ./gdc-test find 3
-> dguid [4e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce]
-> cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
...awaiting reply...
Recv len 100
<- dguid [4e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce]
<- eguid [6b86b273ff34fce19d6b804eff5a3f5747ada4eaa22f1d49c01e52ddb7875b4b]
<- cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
gdp-04[345] ./gdc-test find 16
-> dguid [b17ef6d19c7a5b1ee83b907c595526dcb1eb06db8227d650d5dda0a9f4ce8cd9]
-> cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
...awaiting reply...
Recv len 100
<- dguid [b17ef6d19c7a5b1ee83b907c595526dcb1eb06db8227d650d5dda0a9f4ce8cd9]
<- eguid [6b86b273ff34fce19d6b804eff5a3f5747ada4eaa22f1d49c01e52ddb7875b4b]
<- cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
gdp-04[346] ./gdc-test find 17
-> dguid [4523540f1504cd17100c4835e85b7eefd49911580f8efff0599a8f283be6b9e3]
-> cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
...awaiting reply...
Recv len 100
<- dguid [4523540f1504cd17100c4835e85b7eefd49911580f8efff0599a8f283be6b9e3]
<- eguid nak
gdp-04[347] ./gdc-test remove 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
-> eguid [6b86b273ff34fce19d6b804eff5a3f5747ada4eaa22f1d49c01e52ddb7875b4b]
-> dguid [d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35]
oguid len 448
-> oguid [4e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce]
-> oguid [4b227777d4dd1fc61c6f884f48641d02b4d121d3fd328cb08b5531fcacdabf8a]
-> oguid [ef2d127de37b942baad06145e54b0c619a1f22327b2ebbcfbec78f5564afe39d]
-> oguid [e7f6c011776e8db7cd330b54174fd76f7d0216b612387a5ffcfb81e6f0919683]
-> oguid [7902699be42c8a8e46fbbb4501726517e86b22c56a189f7625a6da49081b2451]
-> oguid [2c624232cdd221771294dfbb310aca000a0df6ac8b66b696d90ef06fdefb64a3]
-> oguid [19581e27de7ced00ff1ce50b2047e7a567c76b1cbaebabe5ef03f7c3017bb5b7]
-> oguid [4a44dc15364204a80fe80e9039455cc1608281820fe2b24f1e5233ade6af1dd5]
-> oguid [4fc82b26aecb47d2868c4efbe3581732a3e7cbcc6c2efb32062c08170a05eeb8]
-> oguid [6b51d431df5d7f141cbececcf79edf3dd861c3b4069f0b11661a3eefacbba918]
-> oguid [3fdba35f04dc8c462986c992bcf875546257113072a909c162f7e470e581e278]
-> oguid [8527a891e224136950ff32ca212b45bc93f69fbb801c3b1ebedac52775f99e61]
-> oguid [e629fa6598d732768f7c726b4b621285f9c3b85303900aa912017db7617d8bdb]
-> oguid [b17ef6d19c7a5b1ee83b907c595526dcb1eb06db8227d650d5dda0a9f4ce8cd9]
Send len 516
...awaiting reply...
Recv len 516
<- dguid [d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35]
oguid len 448
<- oguid [4e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce]
<- oguid [4b227777d4dd1fc61c6f884f48641d02b4d121d3fd328cb08b5531fcacdabf8a]
<- oguid [ef2d127de37b942baad06145e54b0c619a1f22327b2ebbcfbec78f5564afe39d]
<- oguid [e7f6c011776e8db7cd330b54174fd76f7d0216b612387a5ffcfb81e6f0919683]
<- oguid [7902699be42c8a8e46fbbb4501726517e86b22c56a189f7625a6da49081b2451]
<- oguid [2c624232cdd221771294dfbb310aca000a0df6ac8b66b696d90ef06fdefb64a3]
<- oguid [19581e27de7ced00ff1ce50b2047e7a567c76b1cbaebabe5ef03f7c3017bb5b7]
<- oguid [4a44dc15364204a80fe80e9039455cc1608281820fe2b24f1e5233ade6af1dd5]
<- oguid [4fc82b26aecb47d2868c4efbe3581732a3e7cbcc6c2efb32062c08170a05eeb8]
<- oguid [6b51d431df5d7f141cbececcf79edf3dd861c3b4069f0b11661a3eefacbba918]
<- oguid [3fdba35f04dc8c462986c992bcf875546257113072a909c162f7e470e581e278]
<- oguid [8527a891e224136950ff32ca212b45bc93f69fbb801c3b1ebedac52775f99e61]
<- oguid [e629fa6598d732768f7c726b4b621285f9c3b85303900aa912017db7617d8bdb]
<- oguid [b17ef6d19c7a5b1ee83b907c595526dcb1eb06db8227d650d5dda0a9f4ce8cd9]
gdp-04[348] ./gdc-test remove 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 17
-> eguid [6b86b273ff34fce19d6b804eff5a3f5747ada4eaa22f1d49c01e52ddb7875b4b]
-> dguid [d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35]
oguid len 448
-> oguid [4e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce]
-> oguid [4b227777d4dd1fc61c6f884f48641d02b4d121d3fd328cb08b5531fcacdabf8a]
-> oguid [ef2d127de37b942baad06145e54b0c619a1f22327b2ebbcfbec78f5564afe39d]
-> oguid [e7f6c011776e8db7cd330b54174fd76f7d0216b612387a5ffcfb81e6f0919683]
-> oguid [7902699be42c8a8e46fbbb4501726517e86b22c56a189f7625a6da49081b2451]
-> oguid [2c624232cdd221771294dfbb310aca000a0df6ac8b66b696d90ef06fdefb64a3]
-> oguid [19581e27de7ced00ff1ce50b2047e7a567c76b1cbaebabe5ef03f7c3017bb5b7]
-> oguid [4a44dc15364204a80fe80e9039455cc1608281820fe2b24f1e5233ade6af1dd5]
-> oguid [4fc82b26aecb47d2868c4efbe3581732a3e7cbcc6c2efb32062c08170a05eeb8]
-> oguid [6b51d431df5d7f141cbececcf79edf3dd861c3b4069f0b11661a3eefacbba918]
-> oguid [3fdba35f04dc8c462986c992bcf875546257113072a909c162f7e470e581e278]
-> oguid [8527a891e224136950ff32ca212b45bc93f69fbb801c3b1ebedac52775f99e61]
-> oguid [e629fa6598d732768f7c726b4b621285f9c3b85303900aa912017db7617d8bdb]
-> oguid [4523540f1504cd17100c4835e85b7eefd49911580f8efff0599a8f283be6b9e3]
Send len 516
...awaiting reply...
Recv len 516
<- dguid [d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35]
oguid len 448
<- oguid [4e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce]
<- oguid [4b227777d4dd1fc61c6f884f48641d02b4d121d3fd328cb08b5531fcacdabf8a]
<- oguid [ef2d127de37b942baad06145e54b0c619a1f22327b2ebbcfbec78f5564afe39d]
<- oguid [e7f6c011776e8db7cd330b54174fd76f7d0216b612387a5ffcfb81e6f0919683]
<- oguid [7902699be42c8a8e46fbbb4501726517e86b22c56a189f7625a6da49081b2451]
<- oguid [2c624232cdd221771294dfbb310aca000a0df6ac8b66b696d90ef06fdefb64a3]
<- oguid [19581e27de7ced00ff1ce50b2047e7a567c76b1cbaebabe5ef03f7c3017bb5b7]
<- oguid [4a44dc15364204a80fe80e9039455cc1608281820fe2b24f1e5233ade6af1dd5]
<- oguid [4fc82b26aecb47d2868c4efbe3581732a3e7cbcc6c2efb32062c08170a05eeb8]
<- oguid [6b51d431df5d7f141cbececcf79edf3dd861c3b4069f0b11661a3eefacbba918]
<- oguid [3fdba35f04dc8c462986c992bcf875546257113072a909c162f7e470e581e278]
<- oguid [8527a891e224136950ff32ca212b45bc93f69fbb801c3b1ebedac52775f99e61]
<- oguid [e629fa6598d732768f7c726b4b621285f9c3b85303900aa912017db7617d8bdb]
<- oguid [4523540f1504cd17100c4835e85b7eefd49911580f8efff0599a8f283be6b9e3]
gdp-04[349] ./gdc-test find 2
-> dguid [d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35]
-> cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
...awaiting reply...
Recv len 100
<- dguid [d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35]
<- eguid nak
gdp-04[350] ./gdc-test find 3
-> dguid [4e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce]
-> cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
...awaiting reply...
Recv len 100
<- dguid [4e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce]
<- eguid nak
gdp-04[351] ./gdc-test find 16
-> dguid [b17ef6d19c7a5b1ee83b907c595526dcb1eb06db8227d650d5dda0a9f4ce8cd9]
-> cguid [c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1]
...awaiting reply...
Recv len 100
<- dguid [b17ef6d19c7a5b1ee83b907c595526dcb1eb06db8227d650d5dda0a9f4ce8cd9]
<- eguid nak
gdp-04[352] 
