package CocaineDealer;

use strict;
use warnings;

use cocaine_dealer;

sub new
{
	my $class  = shift;
	my $self = __init($_[0]);

	return undef unless $self;

	return bless $self, $class;
}

sub __init
{
	my $dealer = new cocaine_dealer::dealer($_[0]) || return undef;
	return {'dealer' => $dealer};
}

sub send_message
{
	my $self    = shift;
	my $message = shift;
	my $path	= shift;
	my $policy	= shift;

	my $msg_path = new cocaine_dealer::message_path($path -> {"app"},
													$path -> {"event"});
	my $msg_policy;

	if ( not defined $policy ) {
		$msg_policy = new cocaine_dealer::message_policy(0, 0, 0, -1);
	}
	else {
		if ( not defined $policy -> {"urgent"} ) {
		   $policy -> {"urgent"} = 0;
		}

		if ( not defined $policy -> {"timeout"} ) {
		   $policy -> {"timeout"} = 0;
		}

		if ( not defined $policy -> {"deadline"} ) {
		   $policy -> {"deadline"} = 0;
		}

		if ( not defined $policy -> {"max_retries"} ) {
		   $policy -> {"max_retries"} = -1;
		}

		$msg_policy = new cocaine_dealer::message_policy($policy -> {"urgent"},
														 $policy -> {"timeout"},
														 $policy -> {"deadline"},
														 $policy -> {"max_retries"});
	}

	my $response  = new cocaine_dealer::response() || return undef;

	$self -> {'dealer'} -> send_message($message,
										length($message),
										$msg_path,
										$msg_policy,
										$response);

	return $response;
}

1;