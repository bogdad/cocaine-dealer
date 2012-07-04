#!/usr/bin/perl

use lib qw( ../lib ../blib/arch/auto/cocaine_dealer );

require "cocaine_dealer_wrapper.pm";

# create dealer
my $dealer = new CocaineDealer("config.json");

# create message path
my $path = { "app" => "rimz_app", "event" => "rimz_func" };

# send message
my $response = $dealer->send_message("chunk: ", $path);

# create data container to hold chunk data
my $data_container = new cocaine_dealer::data_container;

# fetch chunk while we can
while ($response->get($data_container)) {
	print $data_container->data()."\n";
}

1;
