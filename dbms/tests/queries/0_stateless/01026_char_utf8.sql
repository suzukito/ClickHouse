SELECT char(0xD0, 0xBF, 0xD1, 0x80, 0xD0, 0xB8, 0xD0, 0xB2, 0xD0, 0xB5, 0xD1, 0x82) AS hello;
SELECT char(-48,-65,-47,-128,-48,-72,-48,-78,-48,-75,-47,-126) AS hello;
SELECT char(-48, 0xB0 + number,-47,-128,-48,-72,-48,-78,-48,-75,-47,-126) AS hello FROM numbers(16);
SELECT char(0xe4, 0xbd, 0xa0, 0xe5, 0xa5, 0xbd) AS hello;
