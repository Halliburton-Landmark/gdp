# GDP Directory Daemon Test Client (Interim)

Unit Test:

## Invalid Input

./gdp-nhop add A1
./gdp-nhop add A1 A2 A3
./gdp-nhop find A1
./gdp-nhop find A1 A2 A3

## Valid Input

./gdp-nhop add A1 A2
./gdp-nhop add A2 A3
./gdp-nhop find A1 A3

Final command should find A2 as nhop from A1 to A3.

