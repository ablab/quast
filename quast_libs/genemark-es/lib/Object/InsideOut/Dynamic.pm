package Object::InsideOut; {

use strict;
use warnings;
no warnings 'redefine';

sub create_field
{
    my ($GBL, $call, @args) = @_;
    push(@{$$GBL{'export'}}, 'create_field');
    if ($call eq 'create_field') {
        $$GBL{'init'} = 1;
    }

    # Dynamically create a new object field
    *Object::InsideOut::create_field = sub
    {
        # Handle being called as a method or subroutine
        if ($_[0] eq 'Object::InsideOut') {
            shift;
        }

        my ($class, $field, @attrs) = @_;

        # Verify valid class
        if (! $class->isa('Object::InsideOut')) {
            OIO::Args->die(
                'message' => 'Not an Object::InsideOut class',
                'Arg'     => $class);
        }

        # Check for valid field
        if ($field !~ /^\s*[@%]\s*[a-zA-Z_]\w*\s*$/) {
            OIO::Args->die(
                'message' => 'Not an array or hash declaration',
                'Arg'     => $field);
        }

        # Convert attributes to single string
        my $attr;
        if (@attrs) {
            s/^\s*(.*?)\s*$/$1/ foreach @attrs;
            $attr = join(',', @attrs);
            $attr =~ s/[\r\n]/ /sg;
            $attr =~ s/,\s*,/,/g;
            $attr =~ s/\s*,\s*:/ :/g;
            if ($attr !~ /^\s*:/) {
                $attr = ":Field($attr)";
            }
        } else {
            $attr = ':Field';
        }

        # Create the declaration
        my @errs;
        local $SIG{'__WARN__'} = sub { push(@errs, @_); };

        my $code = "package $class; my $field $attr;";
        eval $code;
        if (my $e = Exception::Class::Base->caught()) {
            die($e);
        }
        if ($@ || @errs) {
            my ($err) = split(/ at /, $@ || join(" | ", @errs));
            OIO::Code->die(
                'message' => 'Failure creating field',
                'Error'   => $err,
                'Code'    => $code);
        }

        # Invalidate object initialization activity cache
        delete($$GBL{'cache'});

        # Process the declaration
        process_fields();
    };


    # Runtime hierarchy building
    *Object::InsideOut::add_class = sub
    {
        my $class = shift;
        if (ref($class)) {
            OIO::Method->die('message' => q/'add_class' called as an object method/);
        }
        if ($class eq 'Object::InsideOut') {
            OIO::Method->die('message' => q/'add_class' called on non-class 'Object::InsideOut'/);
        }
        if (! $class->isa('Object::InsideOut')) {
            OIO::Method->die('message' => "'add_class' called on non-Object::InsideOut class '$class'");
        }

        my $pkg = shift;
        if (! $pkg) {
            OIO::Args->die(
                        'message' => 'Missing argument',
                        'Usage'   => "$class\->add_class(\$class)");
        }

        # Already in the hierarchy - ignore
        return if ($class->isa($pkg));

        no strict 'refs';

        # If no package symbols, then load it
        if (! grep { $_ !~ /::$/ } keys(%{$pkg.'::'})) {
            eval "require $pkg";
            if ($@) {
                OIO::Code->die(
                    'message' => "Failure loading package '$pkg'",
                    'Error'   => $@);
            }
            # Empty packages make no sense
            if (! grep { $_ !~ /::$/ } keys(%{$pkg.'::'})) {
                OIO::Code->die('message' => "Package '$pkg' is empty");
            }
        }

        # Import the package, if needed
        if (@_) {
            eval { $pkg->import(@_); };
            if ($@) {
                OIO::Code->die(
                    'message' => "Failure running 'import' on package '$pkg'",
                    'Error'   => $@);
            }
        }

        my $tree_bu = $$GBL{'tree'}{'bu'};
        my $tree_td = $$GBL{'tree'}{'td'};

        # Foreign class added
        if (! exists($$tree_bu{$pkg})) {
            # Get inheritance 'classes' hash
            if (! exists($$GBL{'heritage'}{$class})) {
                create_heritage($class);
            }
            # Add package to inherited classes
            $$GBL{'heritage'}{$class}{'cl'}{$pkg} = undef;
            return;
        }

        # Add to class trees
        foreach my $cl (keys(%{$tree_bu})) {
            next if (! grep { $_ eq $class } @{$$tree_bu{$cl}});

            # Splice in the added class's tree
            my @tree;
            foreach (@{$$tree_bu{$cl}}) {
                push(@tree, $_);
                if ($_ eq $class) {
                    my %seen;
                    @seen{@{$$tree_bu{$cl}}} = undef;
                    foreach (@{$$tree_bu{$pkg}}) {
                        push(@tree, $_) if (! exists($seen{$_}));
                    }
                }
            }

            # Add to @ISA array
            push(@{$cl.'::ISA'}, $pkg);

            # Save revised trees
            $$tree_bu{$cl} = \@tree;
            @{$$tree_td{$cl}} = reverse(@tree);
        }
        $$GBL{'asi'}{$pkg}{$class} = undef;
    };

    # Invalidate object initialization activity cache
    delete($$GBL{'cache'});

    # Do the original call
    @_ = @args;
    goto &$call;
}

}  # End of package's lexical scope


# Ensure correct versioning
($Object::InsideOut::VERSION eq '3.98')
    or die("Version mismatch\n");

# EOF
