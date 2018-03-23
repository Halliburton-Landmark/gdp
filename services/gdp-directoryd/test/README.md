If this service is ever deployed for real (rather than replaced, which
is the expectation), might be wise to test against a test table rather
than the real table, to avoid disturbing the real table.

Please follow ../README.md to start gdp-directoryd with an appropriate
db set up. Once installed, the gdc-test binary can be used (locally or
remotely) to test the gdp directory service (test cguid is hardwired
0xc1c1c1...):


# extraenous parameters
./gdc-test add eguid_1 dguid_1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
# max parameters
./gdc-test add eguid_1 dguid_1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# check adds
./gdc-test find dguid_1
./gdc-test find 2
./gdc-test find 15
# nak
./gdc-test find 16
./gdc-test remove eguid_1 dguid_1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# extraenous parameters
./gdc-test remove eguid_1 dguid_1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
# naks
./gdc-test find dguid_1
./gdc-test find 2
./gdc-test find 15
./gdc-test find 16


