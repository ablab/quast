package Object::InsideOut; {

use strict;
use warnings;
no warnings 'redefine';

sub install_ATTRIBUTES
{
    my ($GBL) = @_;

    *Object::InsideOut::MODIFY_SCALAR_ATTRIBUTES = sub
    {
        my ($pkg, $scalar, @attrs) = @_;

        # Call attribute handlers in the class tree
        if (exists($$GBL{'attr'}{'MOD'}{'SCALAR'})) {
            @attrs = CHECK_ATTRS('SCALAR', $pkg, $scalar, @attrs);
        }

        # If using Attribute::Handlers, send it any unused attributes
        if (@attrs &&
            Attribute::Handlers::UNIVERSAL->can('MODIFY_SCALAR_ATTRIBUTES'))
        {
            return (Attribute::Handlers::UNIVERSAL::MODIFY_SCALAR_ATTRIBUTES($pkg, $scalar, @attrs));
        }

        # Return any unused attributes
        return (@attrs);
    };

    *Object::InsideOut::CHECK_ATTRS = sub
    {
        my ($type, $pkg, $ref, @attrs) = @_;

        # Call attribute handlers in the class tree
        foreach my $class (@{$$GBL{'tree'}{'bu'}{$pkg}}) {
            if (my $handler = $$GBL{'attr'}{'MOD'}{$type}{$class}) {
                local $SIG{'__DIE__'} = 'OIO::trap';
                @attrs = $handler->($pkg, $ref, @attrs);
                return if (! @attrs);
            }
        }

        return (@attrs);   # Return remaining attributes
    };

    *Object::InsideOut::FETCH_ATTRS = sub
    {
        my ($type, $stash, $ref) = @_;
        my @attrs;

        # Call attribute handlers in the class tree
        if (exists($$GBL{'attr'}{'FETCH'}{$type})) {
            foreach my $handler (@{$$GBL{'attr'}{'FETCH'}{$type}}) {
                local $SIG{'__DIE__'} = 'OIO::trap';
                push(@attrs, $handler->($stash, $ref));
            }
        }

        return (@attrs);
    };

    # Stub ourself out
    *Object::InsideOut::install_ATTRIBUTES = sub { };
}

add_meta('Object::InsideOut', {
    'MODIFY_SCALAR_ATTRIBUTES' => {'hidden' => 1},
    'CHECK_ATTRS'              => {'hidden' => 1},
    'FETCH_ATTRS'              => {'hidden' => 1},
});

sub FETCH_SCALAR_ATTRIBUTES :Sub { return (FETCH_ATTRS('SCALAR', @_)); }
sub FETCH_HASH_ATTRIBUTES   :Sub { return (FETCH_ATTRS('HASH',   @_)); }
sub FETCH_ARRAY_ATTRIBUTES  :Sub { return (FETCH_ATTRS('ARRAY',  @_)); }
sub FETCH_CODE_ATTRIBUTES   :Sub { return (FETCH_ATTRS('CODE',   @_)); }

}  # End of package's lexical scope


# Ensure correct versioning
($Object::InsideOut::VERSION eq '3.98')
    or die("Version mismatch\n");

# EOF
