ece361 coding labs yey

generate file of 0 bytes
`dd if=/dev/zero of=./test.file bs=4k iflag=fullblock,count_bytes count=5M`

make sure to enter the client_dir and server_dir directories before running the executables

/login bryan hello 127.0.0.1 8000
/login bryan hello123 127.0.0.1 8000

/login fu wrongpass 127.0.0.1 8000
/login fu pass123 127.0.0.1 8000

/createsession testsess



netstat -tuln
