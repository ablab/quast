package Object::InsideOut; {

use strict;
use warnings;
no warnings 'redefine';

# Install versions of UNIVERSAL::can/isa that understands :Automethod and
# foreign inheritance
sub install_UNIVERSAL
{
    my ($GBL) = @_;

    *Object::InsideOut::can = sub
    {
        my ($thing, $method) = @_;

        return if (! defined($thing));

        # Metadata call for methods
        if (@_ == 1) {
            my $meths = Object::InsideOut::meta(shift)->get_methods();
            return (wantarray()) ? (keys(%$meths)) : [ keys(%$meths) ];
        }

        return if (! defined($method));

        # First, try the original UNIVERSAL::can()
        my $code;
        if ($method =~ /^SUPER::/) {
            # Superclass WRT caller
            my $caller = caller();
            eval { $code = $thing->Object::InsideOut::SUPER::can($caller.'::'.$method) };
        } else {
            eval { $code = $thing->Object::InsideOut::SUPER::can($method) };
        }
        if ($code) {
            return ($code);
        }

        # Handle various calling methods
        my ($class, $super);
        if ($method !~ /::/) {
            # Ordinary method check
            #   $obj->can('x');
            $class = ref($thing) || $thing;

        } elsif ($method !~ /SUPER::/) {
            # Fully-qualified method check
            #   $obj->can('FOO::x');
            ($class, $method) = $method =~ /^(.+)::([^:]+)$/;

        } elsif ($method =~ /^SUPER::/) {
            # Superclass method check
            #   $obj->can('SUPER::x');
            $class = caller();
            $method =~ s/SUPER:://;
            $super = 1;

        } else {
            # Qualified superclass method check
            #   $obj->can('Foo::SUPER::x');
            ($class, $method) = $method =~ /^(.+)::SUPER::([^:]+)$/;
            $super = 1;
        }

        my $heritage    = $$GBL{'heritage'};
        my $automethods = $$GBL{'sub'}{'auto'};

        # Next, check with heritage objects and Automethods
        my ($code_type, $code_dir, %code_refs);
        foreach my $pkg (@{$$GBL{'tree'}{'bu'}{$class}}) {
            # Skip self's class if SUPER
            if ($super && $class eq $pkg) {
                next;
            }

            # Check heritage
            if (exists($$heritage{$pkg})) {
                no warnings;
                foreach my $pkg2 (keys(%{$$heritage{$pkg}{'cl'}})) {
                    if ($code = $pkg2->can($method)) {
                        return ($code);
                    }
                }
            }

            # Check with the Automethods
            if (my $automethod = $$automethods{$pkg}) {
                # Call the Automethod to get a code ref
                local $CALLER::_ = $_;
                local $_ = $method;
                local $SIG{'__DIE__'} = 'OIO::trap';
                if (my ($code, $ctype) = $automethod->($thing)) {
                    if (ref($code) ne 'CODE') {
                        # Not a code ref
                        OIO::Code->die(
                            'message' => ':Automethod did not return a code ref',
                            'Info'    => ":Automethod in package '$pkg' invoked for method '$method'");
                    }

                    if (defined($ctype)) {
                        my ($type, $dir) = $ctype =~ /(\w+)(?:[(]\s*(.*)\s*[)])?/;
                        if ($type && $type =~ /CUM/i) {
                            if ($code_type) {
                                $type = ':Cumulative';
                                $dir = ($dir && $dir =~ /BOT/i) ? 'bottom up' : 'top down';
                                if ($code_type ne $type || $code_dir ne $dir) {
                                    # Mixed types
                                    my ($pkg2) = keys(%code_refs);
                                    OIO::Code->die(
                                        'message' => 'Inconsistent code types returned by :Automethods',
                                        'Info'    => "Class '$pkg' returned type $type($dir), and class '$pkg2' returned type $code_type($code_dir)");
                                }
                            } else {
                                $code_type = ':Cumulative';
                                $code_dir = ($dir && $dir =~ /BOT/i) ? 'bottom up' : 'top down';
                            }
                            $code_refs{$pkg} = $code;
                            next;
                        }
                        if ($type && $type =~ /CHA/i) {
                            if ($code_type) {
                                $type = ':Chained';
                                $dir = ($dir && $dir =~ /BOT/i) ? 'bottom up' : 'top down';
                                if ($code_type ne $type || $code_dir ne $dir) {
                                    # Mixed types
                                    my ($pkg2) = keys(%code_refs);
                                    OIO::Code->die(
                                        'message' => 'Inconsistent code types returned by :Automethods',
                                        'Info'    => "Class '$pkg' returned type $type($dir), and class '$pkg2' returned type $code_type($code_dir)");
                                }
                            } else {
                                $code_type = ':Chained';
                                $code_dir = ($dir && $dir =~ /BOT/i) ? 'bottom up' : 'top down';
                            }
                            $code_refs{$pkg} = $code;
                            next;
                        }

                        # Unknown automethod code type
                        OIO::Code->die(
                            'message' => "Unknown :Automethod code type: $ctype",
                            'Info'    => ":Automethod in package '$pkg' invoked for method '$method'");
                    }

                    if ($code_type) {
                        # Mixed types
                        my ($pkg2) = keys(%code_refs);
                        OIO::Code->die(
                            'message' => 'Inconsistent code types returned by :Automethods',
                            'Info'    => "Class '$pkg' returned an 'execute immediately' type, and class '$pkg2' returned type $code_type($code_dir)");
                    }

                    # Just a one-shot - return it
                    return ($code);
                }
            }
        }

        if ($code_type) {
            my $tree = ($code_dir eq 'bottom up') ? $$GBL{'tree'}{'bu'} : $$GBL{'tree'}{'td'};
            $code = ($code_type eq ':Cumulative')
                            ? create_CUMULATIVE($method, $tree, \%code_refs)
                            : create_CHAINED($method, $tree, \%code_refs);
            return ($code);
        }

        return;   # Can't
    };


    *Object::InsideOut::isa = sub
    {
        my ($thing, $type) = @_;

        return ('') if (! defined($thing));

        # Metadata call for classes
        if (@_ == 1) {
            return Object::InsideOut::meta($thing)->get_classes();
        }

        # Workaround for Perl bug #47233
        return ('') if (! defined($type));

        # Try original UNIVERSAL::isa()
        if (my $isa = eval { $thing->Object::InsideOut::SUPER::isa($type) }) {
            return ($isa);
        }

        # Next, check heritage
        foreach my $pkg (@{$$GBL{'tree'}{'bu'}{ref($thing) || $thing}}) {
            if (exists($$GBL{'heritage'}{$pkg})) {
                foreach my $pkg (keys(%{$$GBL{'heritage'}{$pkg}{'cl'}})) {
                    if (my $isa = $pkg->isa($type)) {
                        return ($isa);
                    }
                }
            }
        }

        return ('');   # Isn't
    };


    # Stub ourself out
    *Object::InsideOut::install_UNIVERSAL = sub { };
}

}  # End of package's lexical scope


# Ensure correct versioning
($Object::InsideOut::VERSION eq '3.98')
    or die("Version mismatch\n");

# EOF
