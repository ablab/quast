package Object::InsideOut; {

use strict;
use warnings;
no warnings 'redefine';

my $GBL = {};

sub generate_CUMULATIVE :Sub(Private)
{
    ($GBL) = @_;
    my $g_cu = $$GBL{'sub'}{'cumu'};
    my $cumu_td = $$g_cu{'new'}{'td'} || [];
    my $cumu_bu = $$g_cu{'new'}{'bu'} || [];
    delete($$g_cu{'new'});
    if (! exists($$g_cu{'td'})) {
        $$GBL{'sub'}{'cumu'} = {
            td => {},       # 'Top down'
            bu => {},       # 'Bottom up'
            restrict => {}, # :Restricted
        };
        $g_cu = $$GBL{'sub'}{'cumu'};
    }
    my $cu_td    = $$g_cu{'td'};
    my $cu_bu    = $$g_cu{'bu'};
    my $cu_restr = $$g_cu{'restrict'};

    # Get names for :CUMULATIVE methods
    my (%cum_loc);
    while (my $info = shift(@{$cumu_td})) {
        $$info{'name'} ||= sub_name($$info{'code'}, ':CUMULATIVE', $$info{'loc'});
        my $package = $$info{'pkg'};
        my $name    = $$info{'name'};

        $cum_loc{$name}{$package} = $$info{'loc'};

        $$cu_td{$name}{$package} = $$info{'wrap'};
        if (exists($$info{'exempt'})) {
            push(@{$$cu_restr{$package}{$name}},
                    sort grep {$_} split(/[,'\s]+/, $$info{'exempt'} || ''));
        }
    }

    # Get names for :CUMULATIVE(BOTTOM UP) methods
    while (my $info = shift(@{$cumu_bu})) {
        $$info{'name'} ||= sub_name($$info{'code'}, ':CUMULATIVE(BOTTOM UP)', $$info{'loc'});
        my $package = $$info{'pkg'};
        my $name    = $$info{'name'};

        # Check for conflicting definitions of 'name'
        if ($$cu_td{$name}) {
            foreach my $other_package (keys(%{$$cu_td{$name}})) {
                if ($other_package->isa($package) ||
                    $package->isa($other_package))
                {
                    my ($pkg,  $file,  $line)  = @{$cum_loc{$name}{$other_package}};
                    my ($pkg2, $file2, $line2) = @{$$info{'loc'}};
                    OIO::Attribute->die(
                        'location' => $$info{'loc'},
                        'message'  => "Conflicting definitions for cumulative method '$name'",
                        'Info'     => "Declared as :CUMULATIVE in class '$pkg' (file '$file', line $line), but declared as :CUMULATIVE(BOTTOM UP) in class '$pkg2' (file '$file2' line $line2)");
                }
            }
        }

        $$cu_bu{$name}{$package} = $$info{'wrap'};
        if (exists($$info{'exempt'})) {
            push(@{$$cu_restr{$package}{$name}},
                    sort grep {$_} split(/[,'\s]+/, $$info{'exempt'} || ''));
        }
    }

    # Propagate restrictions
    my $reapply = 1;
    my $trees = $$GBL{'tree'}{'td'};
    while ($reapply) {
        $reapply = 0;
        foreach my $pkg (keys(%{$cu_restr})) {
            foreach my $class (keys(%{$trees})) {
                next if (! grep { $_ eq $pkg } @{$$trees{$class}});
                foreach my $p (@{$$trees{$class}}) {
                    foreach my $n (keys(%{$$cu_restr{$pkg}})) {
                        if (exists($$cu_restr{$p}{$n})) {
                            next if ($$cu_restr{$p}{$n} == $$cu_restr{$pkg}{$n});
                            my $equal = (@{$$cu_restr{$p}{$n}} == @{$$cu_restr{$pkg}{$n}});
                            if ($equal) {
                                for (1..@{$$cu_restr{$p}{$n}}) {
                                    if ($$cu_restr{$pkg}{$n}[$_-1] ne $$cu_restr{$p}{$n}[$_-1]) {
                                        $equal = 0;
                                        last;
                                    }
                                }
                            }
                            if (! $equal) {
                                my %restr = map { $_ => 1 } @{$$cu_restr{$p}{$n}}, @{$$cu_restr{$pkg}{$n}};
                                $$cu_restr{$pkg}{$n} = [ sort(keys(%restr)) ];
                                $reapply = 1;
                            }
                        } else {
                            $reapply = 1;
                        }
                        $$cu_restr{$p}{$n} = $$cu_restr{$pkg}{$n};
                    }
                }
            }
        }
    }

    no warnings 'redefine';
    no strict 'refs';

    # Implement :CUMULATIVE methods
    foreach my $name (keys(%{$cu_td})) {
        my $code = create_CUMULATIVE($name, $trees, $$cu_td{$name});
        foreach my $package (keys(%{$$cu_td{$name}})) {
            *{$package.'::'.$name} = $code;
            add_meta($package, $name, 'kind', 'cumulative');
            if (exists($$cu_restr{$package}{$name})) {
                add_meta($package, $name, 'restrict', 1);
            }
        }
    }

    # Implement :CUMULATIVE(BOTTOM UP) methods
    foreach my $name (keys(%{$cu_bu})) {
        my $code = create_CUMULATIVE($name, $$GBL{'tree'}{'bu'}, $$cu_bu{$name});
        foreach my $package (keys(%{$$cu_bu{$name}})) {
            *{$package.'::'.$name} = $code;
            add_meta($package, $name, 'kind', 'cumulative (bottom up)');
            if (exists($$cu_restr{$package}{$name})) {
                add_meta($package, $name, 'restrict', 1);
            }
        }
    }
}


# Returns a closure back to initialize() that is used to setup CUMULATIVE
# and CUMULATIVE(BOTTOM UP) methods for a particular method name.
sub create_CUMULATIVE :Sub(Private)
{
    # $name      - method name
    # $tree      - either $GBL{'tree'}{'td'} or $GBL{'tree'}{'bu'}
    # $code_refs - hash ref by package of code refs for a particular method name
    my ($name, $tree, $code_refs) = @_;

    return sub {
        my $class = ref($_[0]) || $_[0];
        if (! $class) {
            OIO::Method->die('message' => "Must call '$name' as a method");
        }
        my $list_context = wantarray;
        my (@results, @classes);

        # Caller must be in class hierarchy
        my $restr = $$GBL{'sub'}{'cumu'}{'restrict'};
        if ($restr && exists($$restr{$class}{$name})) {
            my $caller = caller();
            if (! ((grep { $_ eq $caller } @{$$restr{$class}{$name}}) ||
                   $caller->isa($class) ||
                   $class->isa($caller)))
            {
                OIO::Method->die('message' => "Can't call restricted method '$class->$name' from class '$caller'");
            }
        }

        # Accumulate results
        foreach my $pkg (@{$$tree{$class}}) {
            if (my $code = $$code_refs{$pkg}) {
                local $SIG{'__DIE__'} = 'OIO::trap';
                my @args = @_;
                if (defined($list_context)) {
                    push(@classes, $pkg);
                    if ($list_context) {
                        # List context
                        push(@results, $code->(@args));
                    } else {
                        # Scalar context
                        push(@results, scalar($code->(@args)));
                    }
                } else {
                    # void context
                    $code->(@args);
                }
            }
        }

        # Return results
        if (defined($list_context)) {
            if ($list_context) {
                # List context
                return (@results);
            }
            # Scalar context - returns object
            return (Object::InsideOut::Results->new('VALUES'  => \@results,
                                                    'CLASSES' => \@classes));
        }
    };
}

}  # End of package's lexical scope


package Object::InsideOut::Results; {

use strict;
use warnings;

our $VERSION = '3.98';
$VERSION = eval $VERSION;

use Object::InsideOut 3.98;
use Object::InsideOut::Metadata 3.98;

my @VALUES  :Field :Arg(VALUES);
my @CLASSES :Field :Arg(CLASSES);
my @HASHES  :Field;

sub as_string :Stringify
{
    return (join('', grep(defined, @{$VALUES[${$_[0]}]})));
}

sub count :Numerify
{
    return (scalar(@{$VALUES[${$_[0]}]}));
}

sub have_any :Boolify
{
    return (@{$VALUES[${$_[0]}]} > 0);
}

sub values :Arrayify
{
    return ($VALUES[${$_[0]}]);
}

sub as_hash :Hashify
{
    my $self = $_[0];

    if (! defined($HASHES[$$self])) {
        my %hash;
        @hash{@{$CLASSES[$$self]}} = @{$VALUES[$$self]};
        $self->set(\@HASHES, \%hash);
    }

    return ($HASHES[$$self]);
}

# Our metadata
add_meta('Object::InsideOut::Results', {
    'new'          => {'hidden' => 1},
    'create_field' => {'hidden' => 1},
    'add_class'    => {'hidden' => 1},
});

}  # End of package's lexical scope


# Ensure correct versioning
($Object::InsideOut::VERSION eq '3.98')
    or die("Version mismatch\n");

# EOF
