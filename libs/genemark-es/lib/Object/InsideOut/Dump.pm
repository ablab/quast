package Object::InsideOut; {

use strict;
use warnings;
no warnings 'redefine';

# Installs object dumper and loader methods
sub dump
{
    my ($GBL, $call, @args) = @_;
    push(@{$$GBL{'export'}}, 'dump');
    $$GBL{'init'} = 1;

    *Object::InsideOut::dump = sub
    {
        my $self = shift;

        my $d_flds =  $$GBL{'dump'}{'fld'};

        # Extract field info from any :InitArgs hashes
        while (my $pkg = shift(@{$$GBL{'dump'}{'args'}})) {
            my $p_args = $$GBL{'args'}{$pkg};
            foreach my $name (keys(%{$p_args})) {
                my $val = $$p_args{$name};
                next if (ref($val) ne 'HASH');
                if (my $field = $$val{'_F'}) {
                    $$d_flds{$pkg} ||= {};
                    if (add_dump_field('InitArgs', $name, $field, $$d_flds{$pkg}) eq 'conflict') {
                        OIO::Code->die(
                            'message' => 'Cannot dump object',
                            'Info'    => "In class '$pkg', '$name' refers to two different fields set by 'InitArgs' and '$$d_flds{$pkg}{$name}{'src'}'");
                    }
                }
            }
        }

        # Must call ->dump() as an object method
        if (! Scalar::Util::blessed($self)) {
            OIO::Method->die('message' => q/'dump' called as a class method/);
        }

        # Gather data from the object's class tree
        my %dump;
        my $fld_refs = $$GBL{'fld'}{'ref'};
        my $dumpers  = $$GBL{'dump'}{'dumper'};
        my $weak     = $$GBL{'fld'}{'weak'};
        foreach my $pkg (@{$$GBL{'tree'}{'td'}{ref($self)}}) {
            # Try to use a class-supplied dumper
            if (my $dumper = $$dumpers{$pkg}) {
                local $SIG{'__DIE__'} = 'OIO::trap';
                $dump{$pkg} = $self->$dumper();

            } elsif ($$fld_refs{$pkg}) {
                # Dump the data ourselves from all known class fields
                my @fields = @{$$fld_refs{$pkg}};

                # Fields for which we have names
                foreach my $name (keys(%{$$d_flds{$pkg}})) {
                    my $field = $$d_flds{$pkg}{$name}{'fld'};
                    if (ref($field) eq 'HASH') {
                        if (exists($$field{$$self})) {
                            $dump{$pkg}{$name} = $$field{$$self};
                        }
                    } else {
                        if (defined($$field[$$self])) {
                            $dump{$pkg}{$name} = $$field[$$self];
                        }
                    }
                    if ($$weak{$field} && exists($dump{$pkg}{$name})) {
                        Scalar::Util::weaken($dump{$pkg}{$name});
                    }
                    @fields = grep { $_ != $field } @fields;
                }

                # Fields for which names are not known
                foreach my $field (@fields) {
                    if (ref($field) eq 'HASH') {
                        if (exists($$field{$$self})) {
                            $dump{$pkg}{$field} = $$field{$$self};
                        }
                    } else {
                        if (defined($$field[$$self])) {
                            $dump{$pkg}{$field} = $$field[$$self];
                        }
                    }
                    if ($$weak{$field} && exists($dump{$pkg}{$field})) {
                        Scalar::Util::weaken($dump{$pkg}{$field});
                    }
                }
            }
        }

        # Package up the object's class and its data
        my $output = [ ref($self), \%dump ];

        # Create a string version of dumped data if arg is true
        if ($_[0]) {
            require Data::Dumper;
            local $Data::Dumper::Indent = 1;
            $output = Data::Dumper::Dumper($output);
            chomp($output);
            $output =~ s/^\$VAR1 = //;  # Remove leading '$VAR1 = '
            $output =~ s/;$//s;         # Remove trailing semi-colon
        }

        # Done - send back the dumped data
        return ($output);
    };


    *Object::InsideOut::pump = sub
    {
        my $input = shift;

        # Check usage
        if ($input) {
            if ($input eq 'Object::InsideOut') {
                $input = shift;    # Called as a class method

            } elsif (Scalar::Util::blessed($input)) {
                OIO::Method->die('message' => q/'pump' called as an object method/);
            }
        }

        # Must have an arg
        if (! $input) {
            OIO::Args->die('message' => 'Missing argument to pump()');
        }

        # Convert string input to array ref, if needed
        if (! ref($input)) {
            my @errs;
            local $SIG{'__WARN__'} = sub { push(@errs, @_); };

            my $array_ref;
            eval "\$array_ref = $input";

            if ($@ || @errs) {
                my ($err) = split(/ at /, $@ || join(" | ", @errs));
                OIO::Args->die(
                    'message'  => 'Failure converting dump string back to hash ref',
                    'Error'    => $err,
                    'Arg'      => $input);
            }

            $input = $array_ref;
        }

        # Check input
        if (ref($input) ne 'ARRAY') {
            OIO::Args->die('message'  => 'Argument to pump() is not an array ref');
        }

        # Extract class name and object data
        my ($class, $dump) = @{$input};
        if (! defined($class) || ref($dump) ne 'HASH') {
            OIO::Args->die('message'  => 'Argument to pump() is invalid');
        }

        # Create a new 'bare' object
        my $self = _obj($class);

        # Store object data
        foreach my $pkg (keys(%{$dump})) {
            if (! exists($$GBL{'tree'}{'td'}{$pkg})) {
                OIO::Args->die('message' => "Unknown class: $pkg");
            }
            my $data = $$dump{$pkg};

            # Try to use a class-supplied pumper
            if (my $pumper = $$GBL{'dump'}{'pumper'}{$pkg}) {
                local $SIG{'__DIE__'} = 'OIO::trap';
                $self->$pumper($data);

            } else {
                # Pump in the data ourselves
                foreach my $fld_name (keys(%{$data})) {
                    my $value = $$data{$fld_name};
                    if (my $field = $$GBL{'dump'}{'fld'}{$pkg}{$fld_name}{'fld'}) {
                        $self->set($field, $value);
                    } else {
                        if ($fld_name =~ /^(?:HASH|ARRAY)/) {
                            OIO::Args->die(
                                'message' => "Unnamed field encounted in class '$pkg'",
                                'Arg'     => "$fld_name => $value");
                        } else {
                            OIO::Args->die(
                                'message' => "Unknown field name for class '$pkg': $fld_name");
                        }
                    }
                }
            }
        }

        # Done - return the object
        return ($self);
    };


    # Do the original call
    @_ = @args;
    goto &$call;
}

}  # End of package's lexical scope


# Ensure correct versioning
($Object::InsideOut::VERSION eq '3.98')
    or die("Version mismatch\n");

# EOF
