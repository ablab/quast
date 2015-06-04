package Object::InsideOut; {

use strict;
use warnings;
no warnings 'redefine';

# Installs foreign inheritance methods
sub inherit
{
    my ($GBL, $call, @args) = @_;
    push(@{$$GBL{'export'}}, qw(inherit heritage disinherit));
    $$GBL{'init'} = 1;

    *Object::InsideOut::inherit = sub
    {
        my $self = shift;

        # Must be called as an object method
        my $obj_class = Scalar::Util::blessed($self);
        if (! $obj_class) {
            OIO::Method->die('message' => q/'inherit' called as a class method/);
        }

        # Inheritance takes place in caller's package
        my $pkg = caller();

        # Restrict usage to inside class hierarchy
        if (! $obj_class->isa($pkg)) {
            OIO::Method->die('message' => "Can't call restricted method 'inherit' from class '$pkg'");
        }

        # Flatten arg list
        my (@arg_objs, $_arg);
        while (defined($_arg = shift)) {
            if (ref($_arg) eq 'ARRAY') {
                push(@arg_objs, @{$_arg});
            } else {
                push(@arg_objs, $_arg);
            }
        }

        # Must be called with at least one arg
        if (! @arg_objs) {
            OIO::Args->die('message' => q/Missing arg(s) to '->inherit()'/);
        }

        # Get 'heritage' field and 'classes' hash
        my $herit = $$GBL{'heritage'};
        if (! exists($$herit{$pkg})) {
            create_heritage($pkg);
        }
        my $objects = $$herit{$pkg}{'obj'};
        my $classes  = $$herit{$pkg}{'cl'};

        # Process args
        my $objs = exists($$objects{$$self}) ? $$objects{$$self} : [];
        while (my $obj = shift(@arg_objs)) {
            # Must be an object
            my $arg_class = Scalar::Util::blessed($obj);
            if (! $arg_class) {
                OIO::Args->die('message' => q/Arg to '->inherit()' is not an object/);
            }
            # Must not be in class hierarchy
            if ($obj_class->Object::InsideOut::SUPER::isa($arg_class) ||
                $arg_class->isa($obj_class))
            {
                OIO::Args->die('message' => q/Args to '->inherit()' cannot be within class hierarchy/);
            }
            # Add arg to object list
            push(@{$objs}, $obj);
            # Add arg class to classes hash
            $$classes{$arg_class} = undef;
        }
        # Add objects to heritage field
        $self->set($objects, $objs);
    };


    *Object::InsideOut::heritage = sub
    {
        my $self = shift;

        # Must be called as an object method
        my $obj_class = Scalar::Util::blessed($self);
        if (! $obj_class) {
            OIO::Method->die('message' => q/'heritage' called as a class method/);
        }

        # Inheritance takes place in caller's package
        my $pkg = caller();

        # Restrict usage to inside class hierarchy
        if (! $obj_class->isa($pkg)) {
            OIO::Method->die('message' => "Can't call restricted method 'heritage' from class '$pkg'");
        }

        # Anything to return?
        if (! exists($$GBL{'heritage'}{$pkg}) ||
            ! exists($$GBL{'heritage'}{$pkg}{'obj'}{$$self}))
        {
            return;
        }

        my @objs;
        if (@_) {
            # Filter by specified classes
            @objs = grep {
                        my $obj = $_;
                        grep { ref($obj) eq $_ } @_
                    } @{$$GBL{'heritage'}{$pkg}{'obj'}{$$self}};
        } else {
            # Return entire list
            @objs = @{$$GBL{'heritage'}{$pkg}{'obj'}{$$self}};
        }

        # Return results
        if (wantarray()) {
            return (@objs);
        }
        if (@objs == 1) {
            return ($objs[0]);
        }
        return (\@objs);
    };


    *Object::InsideOut::disinherit = sub
    {
        my $self = shift;

        # Must be called as an object method
        my $class = Scalar::Util::blessed($self);
        if (! $class) {
            OIO::Method->die('message' => q/'disinherit' called as a class method/);
        }

        # Disinheritance takes place in caller's package
        my $pkg = caller();

        # Restrict usage to inside class hierarchy
        if (! $class->isa($pkg)) {
            OIO::Method->die('message' => "Can't call restricted method 'disinherit' from class '$pkg'");
        }

        # Flatten arg list
        my (@args, $_arg);
        while (defined($_arg = shift)) {
            if (ref($_arg) eq 'ARRAY') {
                push(@args, @{$_arg});
            } else {
                push(@args, $_arg);
            }
        }

        # Must be called with at least one arg
        if (! @args) {
            OIO::Args->die('message' => q/Missing arg(s) to '->disinherit()'/);
        }

        # Get 'heritage' field
        if (! exists($$GBL{'heritage'}{$pkg})) {
            OIO::Code->die(
                'message'  => 'Nothing to ->disinherit()',
                'Info'     => "Class '$pkg' is currently not inheriting from any foreign classes");
        }
        my $objects = $$GBL{'heritage'}{$pkg}{'obj'};

        # Get inherited objects
        my @objs = exists($$objects{$$self}) ? @{$$objects{$$self}} : ();

        # Check that object is inheriting all args
        foreach my $arg (@args) {
            if (Scalar::Util::blessed($arg)) {
                # Arg is an object
                if (! grep { $_ == $arg } @objs) {
                    my $arg_class = ref($arg);
                    OIO::Args->die(
                        'message'  => 'Cannot ->disinherit()',
                        'Info'     => "Object is not inheriting from an object of class '$arg_class' inside class '$class'");
                }
            } else {
                # Arg is a class
                if (! grep { ref($_) eq $arg } @objs) {
                    OIO::Args->die(
                        'message'  => 'Cannot ->disinherit()',
                        'Info'     => "Object is not inheriting from an object of class '$arg' inside class '$class'");
                }
            }
        }

        # Delete args from object
        my @new_list = ();
        OBJECT:
        foreach my $obj (@objs) {
            foreach my $arg (@args) {
                if (Scalar::Util::blessed($arg)) {
                    if ($obj == $arg) {
                        next OBJECT;
                    }
                } else {
                    if (ref($obj) eq $arg) {
                        next OBJECT;
                    }
                }
            }
            push(@new_list, $obj);
        }

        # Set new object list
        if (@new_list) {
            $self->set($objects, \@new_list);
        } else {
            # No objects left
            delete($$objects{$$self});
        }
    };


    *Object::InsideOut::create_heritage = sub
    {
        # Private
        my $caller = caller();
        if ($caller ne 'Object::InsideOut') {
            OIO::Method->die('message' => "Can't call private subroutine 'Object::InsideOut::create_heritage' from class '$caller'");
        }

        my $pkg = shift;

        # Check if 'heritage' already exists
        if (exists($$GBL{'dump'}{'fld'}{$pkg}{'heritage'})) {
            OIO::Attribute->die(
                'message' => "Can't inherit into '$pkg'",
                'Info'    => "'heritage' already specified for another field using '$$GBL{'dump'}{'fld'}{$pkg}{'heritage'}{'src'}'");
        }

        # Create the heritage field
        my $objects = {};

        # Share the field, if applicable
        if (is_sharing($pkg)) {
            threads::shared::share($objects)
        }

        # Save the field's ref
        push(@{$$GBL{'fld'}{'ref'}{$pkg}}, $objects);

        # Save info for ->dump()
        $$GBL{'dump'}{'fld'}{$pkg}{'heritage'} = {
            fld => $objects,
            src => 'Inherit'
        };

        # Save heritage info
        $$GBL{'heritage'}{$pkg} = {
            obj => $objects,
            cl  => {}
        };

        # Set up UNIVERSAL::can/isa to handle foreign inheritance
        install_UNIVERSAL();
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
