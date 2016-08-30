package Object::InsideOut::Secure; {
    use strict;
    use warnings;

    our $VERSION = '3.98';
    $VERSION = eval $VERSION;

    use Object::InsideOut 3.98 ':hash_only';

    # Holds used IDs
    my %used :Field = ( 0 => undef );

    # Our PRNG
    BEGIN {
        $Math::Random::MT::Auto::shared = $threads::shared::threads_shared;
    }
    use Math::Random::MT::Auto 5.04 ':!auto';
    my $prng = Math::Random::MT::Auto->new();

    # Assigns random IDs
    sub _id :ID
    {
        my $id;
        while (exists($used{$id = $prng->irand()})) {}
        $used{$id} = undef;
        return $id;
    }
}

1;

# EOF
