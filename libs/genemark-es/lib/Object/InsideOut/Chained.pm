package Object::InsideOut; {

use strict;
use warnings;
no warnings 'redefine';

my $GBL = {};

sub generate_CHAINED :Sub(Private)
{
    ($GBL) = @_;
    my $g_ch = $$GBL{'sub'}{'chain'};
    my $chain_td = $$g_ch{'new'}{'td'} || [];
    my $chain_bu = $$g_ch{'new'}{'bu'} || [];
    delete($$g_ch{'new'});
    if (! exists($$g_ch{'td'})) {
        $$GBL{'sub'}{'chain'} = {
            td => {},       # 'Top down'
            bu => {},       # 'Bottom up'
            restrict => {}, # :Restricted
        };
        $g_ch = $$GBL{'sub'}{'chain'};
    }
    my $ch_td    = $$g_ch{'td'};
    my $ch_bu    = $$g_ch{'bu'};
    my $ch_restr = $$g_ch{'restrict'};

    # Get names for :CHAINED methods
    my (%chain_loc);
    while (my $info = shift(@{$chain_td})) {
        $$info{'name'} ||= sub_name($$info{'code'}, ':CHAINED', $$info{'loc'});
        my $package = $$info{'pkg'};
        my $name    = $$info{'name'};

        $chain_loc{$name}{$package} = $$info{'loc'};

        $$ch_td{$name}{$package} = $$info{'wrap'};
        if (exists($$info{'exempt'})) {
            push(@{$$ch_restr{$package}{$name}},
                    sort grep {$_} split(/[,'\s]+/, $$info{'exempt'} || ''));
        }
    }

    # Get names for :CHAINED(BOTTOM UP) methods
    while (my $info = shift(@{$chain_bu})) {
        $$info{'name'} ||= sub_name($$info{'code'}, ':CHAINED(BOTTOM UP)', $$info{'loc'});
        my $package = $$info{'pkg'};
        my $name    = $$info{'name'};

        # Check for conflicting definitions of 'name'
        if ($$ch_td{$name}) {
            foreach my $other_package (keys(%{$$ch_td{$name}})) {
                if ($other_package->isa($package) ||
                    $package->isa($other_package))
                {
                    my ($pkg,  $file,  $line)  = @{$chain_loc{$name}{$other_package}};
                    my ($pkg2, $file2, $line2) = @{$$info{'loc'}};
                    OIO::Attribute->die(
                        'location' => $$info{'loc'},
                        'message'  => "Conflicting definitions for chained method '$name'",
                        'Info'     => "Declared as :CHAINED in class '$pkg' (file '$file', line $line), but declared as :CHAINED(BOTTOM UP) in class '$pkg2' (file '$file2' line $line2)");
                }
            }
        }

        $$ch_bu{$name}{$package} = $$info{'wrap'};
        if (exists($$info{'exempt'})) {
            push(@{$$ch_restr{$package}{$name}},
                    sort grep {$_} split(/[,'\s]+/, $$info{'exempt'} || ''));
        }
    }

    # Propagate restrictions
    my $reapply = 1;
    my $trees = $$GBL{'tree'}{'td'};
    while ($reapply) {
        $reapply = 0;
        foreach my $pkg (keys(%{$ch_restr})) {
            foreach my $class (keys(%{$trees})) {
                next if (! grep { $_ eq $pkg } @{$$trees{$class}});
                foreach my $p (@{$$trees{$class}}) {
                    foreach my $n (keys(%{$$ch_restr{$pkg}})) {
                        if (exists($$ch_restr{$p}{$n})) {
                            next if ($$ch_restr{$p}{$n} == $$ch_restr{$pkg}{$n});
                            my $equal = (@{$$ch_restr{$p}{$n}} == @{$$ch_restr{$pkg}{$n}});
                            if ($equal) {
                                for (1..@{$$ch_restr{$p}{$n}}) {
                                    if ($$ch_restr{$pkg}{$n}[$_-1] ne $$ch_restr{$p}{$n}[$_-1]) {
                                        $equal = 0;
                                        last;
                                    }
                                }
                            }
                            if (! $equal) {
                                my %restr = map { $_ => 1 } @{$$ch_restr{$p}{$n}}, @{$$ch_restr{$pkg}{$n}};
                                $$ch_restr{$pkg}{$n} = [ sort(keys(%restr)) ];
                                $reapply = 1;
                            }
                        } else {
                            $reapply = 1;
                        }
                        $$ch_restr{$p}{$n} = $$ch_restr{$pkg}{$n};
                    }
                }
            }
        }
    }

    no warnings 'redefine';
    no strict 'refs';

    # Implement :CHAINED methods
    foreach my $name (keys(%{$ch_td})) {
        my $code = create_CHAINED($name, $trees, $$ch_td{$name});
        foreach my $package (keys(%{$$ch_td{$name}})) {
            *{$package.'::'.$name} = $code;
            add_meta($package, $name, 'kind', 'chained');
            if (exists($$ch_restr{$package}{$name})) {
                add_meta($package, $name, 'restricted', 1);
            }
        }
    }

    # Implement :CHAINED(BOTTOM UP) methods
    foreach my $name (keys(%{$ch_bu})) {
        my $code = create_CHAINED($name, $$GBL{'tree'}{'bu'}, $$ch_bu{$name});
        foreach my $package (keys(%{$$ch_bu{$name}})) {
            *{$package.'::'.$name} = $code;
            add_meta($package, $name, 'kind', 'chained (bottom up)');
            if (exists($$ch_restr{$package}{$name})) {
                add_meta($package, $name, 'restricted', 1);
            }
        }
    }
}


# Returns a closure back to initialize() that is used to setup CHAINED
# and CHAINED(BOTTOM UP) methods for a particular method name.
sub create_CHAINED :Sub(Private)
{
    # $name      - method name
    # $tree      - either $GBL{'tree'}{'td'} or $GBL{'tree'}{'bu'}
    # $code_refs - hash ref by package of code refs for a particular method name
    my ($name, $tree, $code_refs) = @_;

    return sub {
        my $thing = shift;
        my $class = ref($thing) || $thing;
        if (! $class) {
            OIO::Method->die('message' => "Must call '$name' as a method");
        }
        my @args = @_;

        # Caller must be in class hierarchy
        my $restr = $$GBL{'sub'}{'chain'}{'restrict'};
        if ($restr && exists($$restr{$class}{$name})) {
            my $caller = caller();
            if (! ((grep { $_ eq $caller } @{$$restr{$class}{$name}}) ||
                   $caller->isa($class) ||
                   $class->isa($caller)))
            {
                OIO::Method->die('message' => "Can't call restricted method '$class->$name' from class '$caller'");
            }
        }

        # Chain results together
        foreach my $pkg (@{$$tree{$class}}) {
            if (my $code = $$code_refs{$pkg}) {
                local $SIG{'__DIE__'} = 'OIO::trap';
                @args = $thing->$code(@args);
            }
        }

        # Return results
        return (@args);
    };
}

}  # End of package's lexical scope


# Ensure correct versioning
($Object::InsideOut::VERSION eq '3.98')
    or die("Version mismatch\n");

# EOF
