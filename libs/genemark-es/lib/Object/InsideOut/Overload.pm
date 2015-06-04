package Object::InsideOut; {

use strict;
use warnings;
no warnings 'redefine';

sub generate_OVERLOAD :Sub(Private)
{
    my ($GBL) = @_;

    # Overload specifiers
    my %TYPE = (
        'STRINGIFY' => q/""/,
        'NUMERIFY'  => q/0+/,
        'BOOLIFY'   => q/bool/,
        'ARRAYIFY'  => q/@{}/,
        'HASHIFY'   => q/%{}/,
        'GLOBIFY'   => q/*{}/,
        'CODIFY'    => q/&{}/,
    );

    my (%code, $code, %meta);

    # Generate overload strings
    while (my $info = shift(@{$$GBL{'sub'}{'ol'}})) {
        if ($$info{'ify'} eq 'EQUATE') {
            push(@{$code{$$info{'pkg'}}}, "\tq/==/ => sub { (ref(\$_[0]) eq ref(\$_[1])) && (\${\$_[0]} == \${\$_[1]}) },");
        } else {
            $$info{'name'} ||= sub_name($$info{'code'}, ":$$info{'ify'}", $$info{'loc'});
            my $pkg = $$info{'pkg'};
            my $name = $$info{'name'};

            push(@{$code{$pkg}}, "\tq/$TYPE{$$info{'ify'}}/ => sub { \$_[0]->$name() },");

            $meta{$pkg}{$name}{'kind'} = 'overload';
        }
    }
    delete($$GBL{'sub'}{'ol'});

    # Generate entire code string
    foreach my $pkg (keys(%code)) {
        $code .= "package $pkg;\nuse overload (\n" .
                 join("\n", @{$code{$pkg}}) .
                 "\n\t'fallback' => 1);\n";
    }

    # Eval the code string
    my @errs;
    local $SIG{'__WARN__'} = sub { push(@errs, @_); };
    eval $code;
    if ($@ || @errs) {
        my ($err) = split(/ at /, $@ || join(" | ", @errs));
        OIO::Internal->die(
            'message'  => "Failure creating overloads",
            'Error'    => $err,
            'Code'     => $code,
            'self'     => 1);
    }

    # Add accumulated metadata
    add_meta(\%meta);

    no strict 'refs';

    foreach my $pkg (keys(%{$$GBL{'tree'}{'td'}})) {
        # Bless an object into every class
        # This works around an obscure 'overload' bug reported against
        # Class::Std (http://rt.cpan.org/Public/Bug/Display.html?id=14048)
        bless(\do{ my $scalar; }, $pkg);

        # Verify that scalar dereferencing is not overloaded in any class
        if (exists(${$pkg.'::'}{'(${}'})) {
            (my $file = $pkg . '.pm') =~ s/::/\//g;
            OIO::Code->die(
                'location' => [ $pkg, $INC{$file} || '', '' ],
                'message'  => q/Overloading scalar dereferencing '${}' is not allowed/,
                'Info'     => q/The scalar of an object is its object ID, and can't be redefined/);
        }
    }
}

}  # End of package's lexical scope


# Ensure correct versioning
($Object::InsideOut::VERSION eq '3.98')
    or die("Version mismatch\n");

# EOF
