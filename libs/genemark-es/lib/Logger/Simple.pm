package Logger::Simple;
{
  use strict;
  use Carp;
  use FileHandle;
  use Fcntl qw(:flock);
  use Time::HiRes qw/usleep/;
  use vars qw /$VERSION $SEM $ms $SEMAPHORE $FILEHANDLE @HISTORY/;
  use Object::InsideOut;
  
  $ms=750_000;
  $VERSION='2.0';
  $SEM = ".LS.lock";
  $SEMAPHORE=new FileHandle;
  $FILEHANDLE=new FileHandle;
  @HISTORY=();
  
  my @Log         :Field('Standard'=>'Log','Type'=>'LIST');
  my @FileHandle  :Field('Standard'=>'FileHandle','Type'=>'SCALAR');
  my @Semaphore   :Field('Standard'=>'Semaphore','Type'=>'SCALAR');
  my @Error       :Field('Standard'=>'Error','Type'=>'LIST');
  
  my %init_args :InitArgs=(
      'Log'=>{
          'Regex' => qr/^Log$/i,
	  'Mandatory' => 1,
      },
  );
  
  sub _init :Init{
    my($self,$args)= @_;
    if(exists($args->{'Log'})){
      $self->set(\@Log,$args->{'Log'});
    }
    $self->set(\@FileHandle,$FILEHANDLE);
    $self->set(\@Semaphore,$SEMAPHORE);
    $self->open_log;
  }

  sub open_log{
    my $self=shift;
    my $FH=$self->get_FileHandle;
    my $Log=$self->get_Log;
    if(! open($FH,">>$Log")){
      $self->write_error("Unable to open logfile\n");
      return 0; 
    }
    $FH->autoflush(1);
    return 1;
  }

  sub write{
    my($self,$msg)=@_;
    my $FH=$self->get_FileHandle;
    my $format="$0 : [".scalar (localtime)."] $msg";
    ## Fix to ignore locking on Win32
    if($^O eq "MSWin32"){}else{
      $self->lock();
    }
    if(! print $FH "$format\n"){
      croak "Unable to write to log file: $!\n"; 
    }
    if($^O eq "MSWin32"){}else{ 
     $self->unlock();
    }
    $self->update_history($msg);
  }
  
  sub update_history{
    my($self,$msg)=@_;
    push @HISTORY,$msg;
  }

  sub retrieve_history{
    my $self=shift;
    if(wantarray){
      return @HISTORY;
    }else{
      my $message=$HISTORY[$#HISTORY];
      return $message;
    }
  }

  sub lock{
    my $self=shift;
    if($^O eq "MSWin32"){ return 1; }
    my $SM=$self->get_Semaphore;
    open $SM,">$SEM"||die"Can't create lock file: $!\n";
    flock($SM,LOCK_EX) or die"Can't obtain file lock: $!\n";
  }

  sub unlock{
    my $self=shift;
    my $SM=$self->get_Semaphore;
    if(-e $SEM){
      flock($SM,LOCK_UN);
      close $SM;
      $SM->autoflush(1);
      if($^O eq "MSWin32"){ 
        system "C:\\Windows\\System32\\cmd.exe \/c del $SEM";  
      }else{
        unlink $SEM;
      }
    }
  }

  sub wait{
    while(-e $SEM){
     usleep $ms;
    }
  }
  sub clear_history{
    my $self=shift;
    @HISTORY=();
  }
}
1;
__END__

=head1 NAME

Logger::Simple - Implementation of the Simran-Log-Log and Simran-Error-Error modules

=head1 SYNOPSIS

  use Logger::Simple;
  my $log=Logger::Simple->new(LOG=>"/tmp/program.log");
  my $x=5;my $y=4;
  
  if($x>$y){
    $log->write("\$x is greater than \$y");
  }
  
=head1 DESCRIPTION

=over 5

=item new

C<< my $log=Logger::Simple->new(LOG=>"/tmp/logfile"); >>

The new method creates the Logger::Simple object as an inside-out object. The Log parameter
is a mandatory one that must be passed to the object at creation, or the object will fail.
Upon creation, this method will also call the open_log method which opens the log file.

=item write 

C<< $log->write("This is an error message"); >>

This method will write a message to the logfile, and will update the internal
HISTORY array.

=item retrieve_history

C<< my @history = $log->retrieve_history; >>
C<< my $msg = $log->retrieve_history; >>

When called in scalar context, it will return the last message written to the
HISTORY array. When called in a list context, it will return the entire HISTORY
array

=item clear_history

C<< $log->clear_history; >>

This method will clear the internal HISTORY array

=back

=head1 EXPORT

None by default.

=head1 ACKNOWLEDGEMENTS

This module is based on the Simran::Log::Log and Simran::Error::Error
modules. I liked the principle behind them, but felt that the interface
could be a bit better.

My thanks also goes out once again to Damian Conway for Object Oriented Perl,
and also to Sam Tregar, for his book "Writing Perl Modules for CPAN". Both
were invaluable references for me.

I would also like to thank Jerry Heden for his Object::InsideOut module, which
I used to create this module.

=head1 AUTHOR

Thomas Stanley

Thomas_J_Stanley@msn.com

I can also be found on http://www.perlmonks.org as TStanley. You can also
direct any questions concerning this module there as well.

=head1 COPYRIGHT

=begin text

Copyright (C) 2002-2006 Thomas Stanley. All rights reserved. This program is free software; you can distribute it and/or modify it under the same terms as Perl itself.

=end text

=begin html

Copyright E<copy> 2002-2006 Thomas Stanley. All rights reserved. This program is free software; you can distribute it and/or modify it under the same terms as Perl itself.

=end html

=head1 SEE ALSO

perl(1).

Object::InsideOut

Simran::Log::Log

Simran::Error::Error

=cut
