#!/bin/bash

date
echo started in ${DIR}

while true; do
        chips-cli -testnet generatetoaddress 200 '2NEt2aXijaGd8FymGUL7hU71AZw3c1qKCWE' 1000000
done
exec bash
