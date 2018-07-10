# GDP Directory Daemon Test Client (Interim)

Unit Test:

## Invalid Input

./gdp-nhop
./gdp-nhop add
./gdp-nhop add A1
./gdp-nhop add A1 A2 A3
./gdp-nhop delete
./gdp-nhop delete A1
./gdp-nhop delete A1 A2 A3
./gdp-nhop find
./gdp-nhop find A1
./gdp-nhop find A1 A2 A3
./gdp-nhop flush
./gdp-nhop flush A1 A2

## Valid Input

./gdp-nhop add A1 A2
./gdp-nhop add A2 A3
./gdp-nhop add A3 A4
./gdp-nhop add A4 A5

./gdp-nhop find A1 A5
./gdp-nhop flush A4
./gdp-nhop find A1 A5
./gdp-nhop find A1 A4
./gdp-nhop delete A3 A4
./gdp-nhop find A1 A4
./gdp-nhop find A1 A3
