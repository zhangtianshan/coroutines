use IO::Socket::INET;
 
# auto-flush on socket
$| = 1;
 
my $port = 8080;

# creating a listening socket
my $socket = new IO::Socket::INET (
    LocalHost => '0.0.0.0',
    LocalPort => $port,
    Proto => 'tcp',
    Listen => 5,
    Reuse => 1
);
die "cannot create socket $!\n" unless $socket;
print "server waiting for client connection on port $port\n";
 
while(1)
{
    # waiting for a new client connection
    my $client_socket = $socket->accept();
 
    # get information about a newly connected client
    my $client_address = $client_socket->peerhost();
    my $client_port = $client_socket->peerport();
    print "connection from $client_address:$client_port\n";
    my $data = "";
    $data = "hello";
    print "sending $data\n";
    $client_socket->send($data);
 
    # read up to 1024 characters from the connected client
    print "Reading from client...\n";
    $data = "";
    $client_socket->recv($data, 1024);
    print "received data: '$data'\n";
 
    # write response data to the connected client
    $data = "ok";
    $client_socket->send($data);
 
    # notify client that response has been sent
    shutdown($client_socket, 1);
}
 
$socket->close();
