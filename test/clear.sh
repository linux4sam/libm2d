#!/bin/sh

echo | modetest -M atmel-hlcdc -P 27@31:480x272 -d
echo | modetest -M atmel-hlcdc -P 28@31:480x272 -d
echo | modetest -M atmel-hlcdc -P 29@31:480x272 -d
echo | modetest -M atmel-hlcdc -P 30@31:480x272 -d

planes -c ./default.config
