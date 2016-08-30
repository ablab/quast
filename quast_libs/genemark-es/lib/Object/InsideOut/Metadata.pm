package Object::InsideOut::Metadata; {

use strict;
use warnings;

our $VERSION = '3.98';
$VERSION = eval $VERSION;

# Stores method metadata
# Initialized with our own metadata
my %METADATA = (
    'Object::InsideOut::Metadata' => {
        'add_meta'     => {'hidden' => 1},
        'AUTOLOAD'     => {'hidden' => 1},
        'new'          => {'hidden' => 1},
        'create_field' => {'hidden' => 1},
        'add_class'    => {'hidden' => 1},
        'get_classes'  => {'kind' => 'object'},
        'get_methods'  => {'kind' => 'object'},
        'get_args'     => {'kind' => 'object'},
    },
);


### Exported Subroutines ###

sub import
{
    # Export 'add_meta' call
    no strict 'refs';
    my $caller = caller();
    *{$caller.'::add_meta'} = \&add_meta;
    $METADATA{$caller}{'add_meta'}{'hidden'} = 1;
}


# Stores metadata for later use
sub add_meta
{
    my ($class, $name, $meta, $value) = @_;

    if (@_ == 4) {
        $METADATA{$class}{$name}{$meta} = $value;
    } else {
        my $data;
        if (@_ == 3) {
            $$data{$class}{$name} = $meta;
        } elsif (@_ == 2) {
            $$data{$class} = $name;
        } else {
            $data = $class;
        }
        while (my ($c, $mn) = each(%$data)) {
            while (my ($n, $md) = each(%$mn)) {
                while (my ($m, $v) = each(%$md)) {
                    $METADATA{$c}{$n}{$m} = $v;
                }
            }
        }
    }
}


# This will load the OO option of our code.
# It's done this way because of circular dependencies with OIO.
sub AUTOLOAD
{
    # Need 5.8.0 or later
    if ($] < 5.008) {
        OIO::Code->die('message' => q/Introspection API requires Perl 5.8.0 or later/,
                       'ignore_package' => 'Object::InsideOut::Metadata');
    }

    # It's a bug if not invoked by ->new()
    # This should only ever happen once
    if (our $AUTOLOAD ne 'Object::InsideOut::Metadata::new') {
        OIO::Method->die('message' => "Object::InsideOut::Metadata does not support AUTOLOAD of $AUTOLOAD",
                         'ignore_package' => 'Object::InsideOut::Metadata');
    }

    # Workaround to get %METADATA into our scope
    my $meta = \%METADATA;

    # Load the rest of our code
    my $text = do { local $/; <DATA> };
    close(DATA);
    eval $text;
    die if $@;

    # Continue on
    goto &Object::InsideOut::new;
}

}  # End of package's lexical scope

1;

__DATA__

### Object Interface ###

use Object::InsideOut 3.98;

my @CLASSES :Field;
my @FOREIGN :Field;

my $GBL;

my %init_args :InitArgs = (
    'GBL'   => '',
    'CLASS' => '',
);

sub _init :Init
{
    my ($self, $args) = @_;

    $GBL = $args->{'GBL'};

    my $class = $args->{'CLASS'};
    $CLASSES[$$self] = $$GBL{'tree'}{'td'}{$class};

    my %foreign;
    my $herit = $$GBL{'heritage'};
    foreach my $pkg (@{$$GBL{'tree'}{'bu'}{$class}}) {
        if (exists($$herit{$pkg})) {
            @foreign{keys(%{$$herit{$pkg}{'cl'}})} = undef;
        }
    }
    $FOREIGN[$$self] = [ keys(%foreign) ];
}


# Class list
sub get_classes
{
    my $self = shift;
    if (! Scalar::Util::blessed($self)) {
        OIO::Method->die('message' => q/'get_classes' called as a class method/);
    }
    my @classes = (@{$CLASSES[$$self]}, @{$FOREIGN[$$self]});
    return ((wantarray()) ? @classes : \@classes);
}


# Contructor argument list
sub get_args
{
    my $self = shift;

    if (! Scalar::Util::blessed($self)) {
        OIO::Method->die('message' => q/'get_args' called as a class method/);
    }

    my %args;
    foreach my $pkg (@{$CLASSES[$$self]}) {
        if (my $ia = $$GBL{'args'}{$pkg}) {
            foreach my $arg (keys(%$ia)) {
                next if ($arg eq ' ');
                my $hash = $$ia{$arg};
                $args{$pkg}{$arg} = {};
                if (ref($hash) eq 'HASH') {
                    if ($$hash{'_F'}) {
                        $args{$pkg}{$arg}{'field'} = 1;
                    }
                    if ($$hash{'_M'}) {
                        $args{$pkg}{$arg}{'mandatory'} = 1;
                    }
                    if (defined(my $def = $$hash{'_D'})) {
                        $args{$pkg}{$arg}{'default'} = Object::InsideOut::Util::clone($def);
                    }
                    if (my $pre = $$hash{'_P'}) {
                        $args{$pkg}{$arg}{'preproc'} = $pre;
                    }
                    if (my $type = $$hash{'_T'}) {
                        if (!ref($type)) {
                            $type =~ s/\s//g;
                            my $subtype;
                            if ($type =~ /^(.*)\((.+)\)$/i) {
                                $type = $1;
                                $subtype = $2;
                                if ($subtype =~ /^num(?:ber|eric)?$/i) {
                                    $subtype = 'numeric';
                                }
                            }
                            if ($type =~ /^num(?:ber|eric)?$/i) {
                                $type = 'numeric';
                            } elsif ($type =~ /^(?:list|array)$/i) {
                                $type = 'list';
                            } elsif ($type =~ /^(array|hash)(?:_?ref)?$/i) {
                                $type = uc($1);
                            }
                            if ($subtype) {
                                $type .= "($subtype)";
                            }
                        }
                        $args{$pkg}{$arg}{'type'} = $type;
                    }
                }
            }
        }
    }

    return (wantarray() ? %args : \%args);
}


# Available methods
sub get_methods
{
    my $self = shift;

    if (! Scalar::Util::blessed($self)) {
        OIO::Method->die('message' => q/'get_methods' called as a class method/);
    }

    my %methods;
    foreach my $pkg (@{$FOREIGN[$$self]}, 'Object::InsideOut', @{$CLASSES[$$self]}) {
        my $foreign = grep { $_ eq $pkg } @{$FOREIGN[$$self]};

        # Get all subs
        no strict 'refs';
        foreach my $sym (keys(%{$pkg.'::'})) {
            next if (! *{$pkg.'::'.$sym}{'CODE'});
            next if ($sym =~ /^[(_]/);    # '(' overload; '_' private
            next if ($sym =~ /^(?:CLONE(?:_SKIP)?|DESTROY|import)$/);

            $methods{$sym}{'class'} = $pkg;
            if ($foreign) {
                $methods{$sym}{'kind'} = 'foreign';
            }
        }

        if ($METADATA{$pkg}) {
            foreach my $meth (keys(%{$METADATA{$pkg}})) {
                # Remove hidden methods
                if ($METADATA{$pkg}{$meth}{'hidden'}) {
                    delete($methods{$meth});
                    next;
                }
            }
        }

        if ($$GBL{'sub'}{'auto'}{$pkg}) {
            $methods{'AUTOLOAD'} = { 'kind'  => 'automethod',
                                     'class' => $pkg };
        }
    }

    # Add metadata
    foreach my $meth (keys(%methods)) {
        next if ($meth eq 'AUTOLOAD');
        my $pkg = $methods{$meth}{'class'};
        if ($METADATA{$pkg}) {
            foreach my $key (keys(%{$METADATA{$pkg}{$meth}})) {
                $methods{$meth}{$key} = $METADATA{$pkg}{$meth}{$key};
            }
        }
    }

    return (wantarray() ? %methods : \%methods);
}


=head1 NAME

Object::InsideOut::Metadata - Introspection for Object::InsideOut classes

=head1 VERSION

This document describes Object::InsideOut::Metadata version 3.98

=head1 SYNOPSIS

 package My::Class; {
     use Object::InsideOut;
     use Object::InsideOut::Metadata;

     my @data :Field :Arg('data') :Get('data') :Set('put_data');
     my @misc :Field;

     my %init_args :InitArgs = (
         'INFO' => '',
     );

     sub _init :Init
     {
         my ($self, $args) = @_;
         if (exists($args->{'INFO'})) {
             $misc[$$self] = 'INFO: ' . $args->{'INFO'};
         }
     }

     sub misc :lvalue :Method
     {
         my $self = shift;
         $misc[$$self];
     }
     add_meta(__PACKAGE__, 'misc', { 'lvalue' => 1 });
 }

 package main;

 # Obtain a metadata object for a class
 my $meta = My::Class->meta();

 # ... or obtain a metadata object for an object
 my $obj = My::Class->new();
 my $meta = $obj->meta();

 # Obtain the class hierarchy from the metadata object
 my @classes = $meta->get_classes();

 # Obtain infomation on the parameters for a class's construction
 my %args = $meta->get_args();

 # Obtain information on a class's methods
 my %methods = $meta->get_methods();

=head1 DESCRIPTION

Object::InsideOut provides an introspection API that allows you to obtain
metadata on a class's hierarchy, constructor parameters, and methods.  This is
done through the use of metadata objects that are generated by this class.

In addition, developers can specify metadata data for methods they write for
their classes.

=head1 METADATA OBJECT

To obtain metadata on an Object::InsideOut class or object, you must first
generate a metadata object for that class or object.  Using that metadata
object, one can then obtain information on the class hierarchy, constructor
parameters, and methods.

=over

=item my $meta = My::Class->meta();

=item my $meta = $obj->meta();

The C<-E<gt>meta()> method, which is exported by Object::InsideOut to each
class, returns an L<Object::InsideOut::Metadata> object which can then be
I<queried> for information about the invoking class or invoking object's
class.

=back

=head1 CLASS HIERARCHY

Any Object::InsideOut class potentially has four categories of classes
associated with it:

=over

=item 1.  Object::InsideOut

While the basis for all Object::InsideOut classes it is not an object class
per se because you can create objects from it (i.e., you can't do
C<Object::InsideOut->new()>).  While C<My::Class->isa('Object::InsideOut')>
will return I<true>, because Object::InsideOut is not an object class, it is
not considered to be part of a class's hierarchy.

=item 2.  The class itself

A class's hierarchy always includes itself.

=item 3.  Parent classes

These are all the Object::InsideOut classes up the inheritance tree that a
class is derived from.

=item 4.  Foreign classes

These are non-Object::InsideOut classes that a class inherits from.  (See
L<Object::InsideOut/"FOREIGN CLASS INHERITANCE">.)  Because of implementation
details, foreign classes do not appear in a class's C<@ISA> array.  However,
Object::InsideOut implements a version of C<-E<gt>isa()> that handles foreign
classes.

=back

A class's hierarchy consists of any classes in the latter three categories.

=over

=item $meta->get_classes();

When called in an array context, returns a list that constitutes the class
hierarchy for the class or object used to generate the metadata object.  When
called in a scalar context, returns an array ref.

=item My::Class->isa();

=item $obj->isa();

When called in an array context, calling C<-E<gt>isa()> without any arguments
on an Object::InsideOut class or object returns a list of the classes in the
class hierarchy for that class or object, and is equivalent to:

 my @classes = $obj->meta()->get_classes();

When called in a scalar context, it returns an array ref containing the
classes.

=back

=head1 CONSTRUCTOR PARAMETERS

Constructor parameters are the arguments given to a class's C<-E<gt>new()>
call.

=over

=item $meta->get_args();

Returns a hash (hash ref in scalar context) containing information on the
parameters that can be used to construct an object from the class associated
with the metadata object.  Here's an example of such a hash:

 {
     'My::Class' => {
         'data' => {
             'field' => 1,
             'type' => 'numeric',
         },
         'misc' => {
             'mandatory' => 1,
         },
     },
     'My::Parent' => {
         'info' => {
             'default' => '<none>',
         },
     },
 }

The keys for this hash are the Object::IsideOut classes in the class
hierarchy.  These I<class keys> are paired with hash refs, the keys of which
are the names of the parameters for that class (e.g., 'data' and 'misc' for
My::Class, and 'info' for My::Parent).  The hashes paired to the parameters
contain information about the parameter:

=over

=item field

The parameter corresponds directly to a class field, and is automatically
processed during object creation.
See L<Object::InsideOut/"Field-Specific Parameters">.

=item mandatory

The parameter is required for object creation.
See L<Object::InsideOut/"Mandatory Parameters">.

=item default

The default value assigned to the parameter if it is not found in the
arguments to C<-E<gt>new()>.
See L<Object::InsideOut/"Default Values">.

=item preproc

The code ref for the subroutine that is used to I<preprocess> a parameter's
value.
See L<Object::InsideOut/"Parameter Preprocessing">

=item type

The form of type checking performed on the parameter.
See L<Object::InsideOut/"TYPE CHECKING"> for more details.

=over

=item 'numeric'

Parameter takes a numeric value as recognized by
L<Scalar::Util::looks_like_number()|Scalar::Util/"looks_like_number EXPR">.

=item 'list'

=item 'list(_subtype_)'

Parameter takes a single value (which is then placed in an array ref) or an
array ref.

When specified, the contents of the resulting array ref must be of the
specified subtype:

=over

=item 'numeric'

Same as for the basic type above.

=item A class name

Same as for the basic type below.

=item A reference type

Any reference type as returned by L<ref()|perlfunc/"ref EXPR">).

=back

=item 'ARRAY(_subtype_)'

Parameter takes an array ref with contents of the specified subtype as per the
above.

=item A class name

Parameter takes an object of a specified class, or one of its sub-classes as
recognized by C<-E<gt>isa()>.

=item Other reference type

Parameter takes a reference of the specified type as returned by
L<ref()|perlfunc/"ref EXPR">.

=item A code ref

Parameter takes a value that is type-checked by the code ref paired to the
'type' key.

=back

=back

=back

=head1 METHODS METADATA

The methods returned by a metadata object are those that are currently
available at the time of the C<-E<gt>get_methods()> call.

The presence of C<:Automethod> subroutines in an Object::InsideOut class, or
C<AUTOLOAD> in a foreign class means that the methods supported by the class
may not be determinable.  The presence of C<AUTOLOAD> in the list of methods
for a class should alert the programmer to the fact that more methods may be
supported than are listed.

Methods that are excluded are private and hidden methods (see
L<Object::InsideOut/"PERMISSIONS">), methods that begin with an underscore
(which, by convention, means they are private), and subroutines named
C<CLONE>, C<CLONE_SKIP>, and C<DESTROY> (which are not methods).  While
technically a method, C<import> is also excluded as it is generally not
invoked directly (i.e., it's usually called as part of C<use>).

=over

=item $meta->get_methods();

Returns a hash (hash ref in scalar context) containing information on the
methods for the class associated with the metadata object.  The keys in the
hash are the method names.  Paired to the names are hash refs containing
metadata about the methods.  Here's an example:

 {
     # Methods exported by Object::InsideOut
     'new' => {
        'class' => 'My::Class',
        'kind'  => 'constructor'
     },
     'clone' => {
         'class' => 'My::Class',
         'kind'  => 'object'
     },
     'meta'  => {
         'class' => 'My::Class'
     },
     'set' => {
         'class' => 'My::Class',
         'kind'  => 'object',
         'restricted' => 1
     },
     # Methods provided by Object::InsideOut
     'dump' => {
         'class' => 'Object::InsideOut',
         'kind'  => 'object'
     },
     'pump' => {
         'class' => 'Object::InsideOut',
         'kind'  => 'class'
     },
     'inherit' => {
         'class' => 'Object::InsideOut',
         'kind'  => 'object',
         'restricted' => 1
     },
     'heritage' => {
         'class' => 'Object::InsideOut',
         'kind'  => 'object',
         'restricted' => 1
     },
     'disinherit' => {
         'class' => 'Object::InsideOut',
         'kind'  => 'object',
         'restricted' => 1
     },
     # Methods generated by Object::InsideOut for My::Class
     'set_data' => {
         'class'  => 'My::Class',
         'kind'   => 'set',
         'type'   => 'ARRAY',
         'return' => 'new'
     },
     'get_data' => {
         'class' => 'My::Class',
         'kind'  => 'get'
     }
     # Class method provided by My::Class
     'my_method' => {
         'class' => 'My::Class',
         'kind'  => 'class'
     }
 }

Here are the method metadata that are provided:

=over

=item class

The class in whose symbol table the method resides.  The method may reside in
the classes code, it may be exported by another class, or it may be generated
by Object::InsideOut.

Methods that are overridden in child classes are represented as being
associated with the most junior class for which they appear.

=item kind

Designation of the I<characteristic> of the method:

=over

=item constructor

The C<-E<gt>new()> method, of course.

=item get, set or accessor

A I<get>, I<set>, or I<combined> accessor generated by Object::InsideOut.
See L<Object::InsideOut/"AcCESSOR GENERATION">.

=item cumulative, or cumulative (bottom up)

=item chained, or chained (bottom up)

A cumulative or chained method.  See L<Object::InsideOut/"CUMULATIVE
METHODS">, and L<Object::InsideOut/"CHAINED METHODS">.  The class associated
with these methods is the most junior class in which they appears.

=item class

A method that is callable only on a class (e.g.,
C<My::Class-E<gt>my_method()>).

=item object

A method that is callable only on a object (e.g. C<$obj-E<gt>get_data()>).

=item foreign

A subroutine found in a foreign class's symbol table.  Programmers must check
the class's documentation to determine which are actually methods, and what
kinds of methods they are.

=item overload

A subroutine used for L<object coercion|Object::InsideOut/"OBJECT COERCION">.
These may be called as methods, but this is not normally how they are used.

=item automethod

Associated with an AUTOLOAD method for an Object::InsideOut class that
implements an C<:Automethod> subroutine.
See L<Object::InsideOut/"AUTOMETHODS">.

=back

=item type

The type checking that is done on arguments to I<set/combined> accessors
generated by Object::InsideOut.
See L<Object::InsideOut/"TYPE CHECKING">

=item return

The value returned by a I<set/combined> accessor generated by
Object::InsideOut.
See L<Object::InsideOut/"I<Set> Accessor Return Value">

=item lvalue

The method is an L<:lvalue accessor|Object::InsideOut/":lvalue Accessors">.

=item restricted

The method is I<restricted> (i.e., callable only from within the class
hierarchy; not callable from application code).
See L<Object::InsideOut/"PERMISSIONS">.

=back

=item My::Class->can();

=item $obj->can();

When called in an array context, calling C<-E<gt>can()> without any arguments
on an Object::InsideOut class or object returns a list of the method names for
that class or object, and is equivalent to:

 my %meths = $obj->meta()->get_methods();
 my @methods = keys(%meths);

When called in a scalar context, it returns an array ref containing the
method names.

=back

=head2 METADATA ATTRIBUTES

Class authors may add the C<:Method> attribute to subroutines in their classes
to specifically designate them as OO-callable methods.  If a method is only a
I<class> method or only an I<object> method, this may be added as a parameter
to the attribute:

 sub my_method :Method(class)
 {
     ...

The I<class> or I<object> parameter will appear in the metadata for the method
when listed using C<-E<gt>get_methods()>.

B<CAUTION:>  Be sure not to use C<:method> (all lowercase) except as
appropriate (see L<Object::InsideOut/"ARGUMENT VALIDATION">) as this is a Perl
reserved attribute.

The C<:Sub> attribute can be used to designate subroutines that are not
OO-callable methods.  These subroutines will not show up as part of the
methods listed by C<-E<gt>get_methods()>, etc..

Subroutine names beginning with an underscore are, by convention, considered
private, and will not show up as part of the methods listed by
C<-E<gt>get_methods()>, etc..

=head2 ADDING METADATA

Class authors may add additional metadata to their methods using the
C<add_meta()> subroutine which is exported by this package.  For example, if
the class implements it own C<:lvalue> method, it should add that metadata so
that it is picked up the C<-E<gt>get_methods()>:

 package My::Class; {
     use Object::InsideOut;
     use Object::InsideOut::Metadata;

     sub my_method :lvalue :Method(object)
     {
         ....
     }
     add_meta(__PACKAGE__, 'my_method', 'lvalue', 1);
 }

The arguments to C<add_meta()> are:

=over

=item Class name

This can usually be designated using the special literal C__PACKAGE__>.

=item Method name

=item Metadata name

This can be any of the metadata names under L</"METHODS METADATA">, or can be
whatever additional name the programmer chooses to implement.

=item Metadata value

=back

When adding multiple metadata for a method, they may be enclosed in a single
hash ref:

 add_meta(__PACKAGE__, 'my_method', { 'lvalue' => 1,
                                      'return' => 'old' });

If adding metadata for multiple methods, another level of hash may be used:

 add_meta(__PACKAGE__, { 'my_method' => { 'lvalue' => 1,
                                          'return' => 'old' },
                         'get_info'  => { 'my_meta' => 'true' } });

=head1 TO DO

Provide filtering capabilities on the method information returned by
C<-E<gt>get_methods()>.

=head1 REQUIREMENTS

Perl 5.8.0 or later

=head1 SEE ALSO

L<Object::InsideOut>

Perl 6 introspection:
L<http://dev.perl.org/perl6/doc/design/apo/A12.html#Introspection>, and
L<http://dev.perl.org/perl6/rfc/335.html>

=head1 AUTHOR

Jerry D. Hedden, S<E<lt>jdhedden AT cpan DOT orgE<gt>>

=head1 COPYRIGHT AND LICENSE

Copyright 2006 - 2012 Jerry D. Hedden. All rights reserved.

This program is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut
