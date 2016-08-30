package Statistics::LineFit;
use strict;
use Carp qw(carp);
BEGIN {
        use Exporter ();
        use vars qw ($AUTHOR $VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
        $AUTHOR      = 'Richard Anderson <cpan(AT)richardanderson(DOT)org>';
        @EXPORT      = @EXPORT_OK = qw();
        %EXPORT_TAGS = ();
        @ISA         = qw(Exporter);
        $VERSION     = 0.06;
}

sub new {
#
# Purpose: Create a new Statistics::LineFit object
#
    my ($caller, $validate, $hush) = @_;
    my $self = { doneRegress  => 0,
                 gotData      => 0,
                 hush         => defined $hush ? $hush : 0,
                 validate     => defined $validate ? $validate : 0,
               };
    bless $self, ref($caller) || $caller;
    return $self;
}

sub coefficients {
#
# Purpose: Return the slope and intercept from least squares line fit
# 
    my $self = shift;
    unless (defined $self->{intercept} and defined $self->{slope}) {
        $self->regress() or return;
    }
    return ($self->{intercept}, $self->{slope});
}

sub computeSums {
#
# Purpose: Compute sum of x, y, x**2, y**2 and x*y (private method)
#
    my $self = shift;
    my ($sumX, $sumY, $sumXX, $sumYY, $sumXY) = (0, 0, 0, 0, 0);
    if (defined $self->{weight}) {
        for (my $i = 0; $i < $self->{numXY}; ++$i) {
            $sumX += $self->{weight}[$i] * $self->{x}[$i];
            $sumY += $self->{weight}[$i] * $self->{y}[$i];
            $sumXX += $self->{weight}[$i] * $self->{x}[$i] ** 2;
            $sumYY += $self->{weight}[$i] * $self->{y}[$i] ** 2;
            $sumXY += $self->{weight}[$i] * $self->{x}[$i] 
                * $self->{y}[$i];
        }
    } else {
        for (my $i = 0; $i < $self->{numXY}; ++$i) {
            $sumX += $self->{x}[$i];
            $sumY += $self->{y}[$i];
            $sumXX += $self->{x}[$i] ** 2;
            $sumYY += $self->{y}[$i] ** 2;
            $sumXY += $self->{x}[$i] * $self->{y}[$i];
        }
    }
    return ($sumX, $sumY, $sumXX, $sumYY, $sumXY);
}

sub durbinWatson {
#
# Purpose: Return the Durbin-Watson statistic
# 
    my $self = shift;
    unless (defined $self->{durbinWatson}) {
        $self->regress() or return;
        my $sumErrDiff = 0;
        my $errorTMinus1 = $self->{y}[0] - ($self->{intercept} + $self->{slope}
            * $self->{x}[0]);
        for (my $i = 1; $i < $self->{numXY}; ++$i) {
            my $error = $self->{y}[$i] - ($self->{intercept} + $self->{slope}
                * $self->{x}[$i]);
            $sumErrDiff += ($error - $errorTMinus1) ** 2;
            $errorTMinus1 = $error;
        }
        $self->{durbinWatson} = $self->sumSqErrors() > 0 ?
            $sumErrDiff / $self->sumSqErrors() : 0;
    }
    return $self->{durbinWatson};
}

sub meanSqError {
#
# Purpose: Return the mean squared error
# 
    my $self = shift;
    unless (defined $self->{meanSqError}) {
        $self->regress() or return;
        $self->{meanSqError} = $self->sumSqErrors() / $self->{numXY};
    }
    return $self->{meanSqError};
}

sub predictedYs {
#
# Purpose: Return the predicted y values
# 
    my $self = shift;
    unless (defined $self->{predictedYs}) {
        $self->regress() or return;
        $self->{predictedYs} = [];
        for (my $i = 0; $i < $self->{numXY}; ++$i) {
            $self->{predictedYs}[$i] = $self->{intercept} 
                + $self->{slope} * $self->{x}[$i];
        }
    }
    return @{$self->{predictedYs}};
}

sub regress {
#
# Purpose: Do weighted or unweighted least squares 2-D line fit (if needed)
#
# Description:
# The equations below apply to both the weighted and unweighted fit: the
# weights are normalized in setWeights(), so the sum of the weights is
# equal to numXY.
# 
    my $self = shift;
    return $self->{regressOK} if $self->{doneRegress};
    unless ($self->{gotData}) {
        carp "No valid data input - can't do regression" unless $self->{hush};
        return 0;
    } 
    my ($sumX, $sumY, $sumYY, $sumXY);
    ($sumX, $sumY, $self->{sumXX}, $sumYY, $sumXY) = $self->computeSums();
    $self->{sumSqDevX} = $self->{sumXX} - $sumX ** 2 / $self->{numXY};
    if ($self->{sumSqDevX} != 0) {
        $self->{sumSqDevY} = $sumYY - $sumY ** 2 / $self->{numXY};
        $self->{sumSqDevXY} = $sumXY - $sumX * $sumY / $self->{numXY};
        $self->{slope} = $self->{sumSqDevXY} / $self->{sumSqDevX};
        $self->{intercept} = ($sumY - $self->{slope} * $sumX) / $self->{numXY};
        $self->{regressOK} = 1;
    } else {
        carp "Can't fit line when x values are all equal" unless $self->{hush};
        $self->{sumXX} = $self->{sumSqDevX} = undef;
        $self->{regressOK} = 0;
    }
    $self->{doneRegress} = 1;
    return $self->{regressOK};
}

sub residuals {
#
# Purpose: Return the predicted Y values minus the observed Y values
# 
    my $self = shift;
    unless (defined $self->{residuals}) {
        $self->regress() or return;
        $self->{residuals} = [];
        for (my $i = 0; $i < $self->{numXY}; ++$i) {
            $self->{residuals}[$i] = $self->{y}[$i] - ($self->{intercept} 
                + $self->{slope} * $self->{x}[$i]);
        }
    }
    return @{$self->{residuals}};
}

sub rSquared {
#
# Purpose: Return the correlation coefficient
# 
    my $self = shift;
    unless (defined $self->{rSquared}) {
        $self->regress() or return;
        my $denom = $self->{sumSqDevX} * $self->{sumSqDevY};
        $self->{rSquared} = $denom != 0 ? $self->{sumSqDevXY} ** 2 / $denom : 1;
    }
    return $self->{rSquared};
}

sub setData {
#
# Purpose: Initialize (x,y) values and optional weights
# 
    my ($self, $x, $y, $weights) = @_;
    $self->{doneRegress} = 0;
    $self->{x} = $self->{y} = $self->{numXY} = $self->{weight} 
        = $self->{intercept} = $self->{slope} = $self->{rSquared} 
        = $self->{sigma} = $self->{durbinWatson} = $self->{meanSqError} 
        = $self->{sumSqErrors} = $self->{tStatInt} = $self->{tStatSlope} 
        = $self->{predictedYs} = $self->{residuals} = $self->{sumXX} 
        = $self->{sumSqDevX} = $self->{sumSqDevY} = $self->{sumSqDevXY} 
        = undef;
    if (@$x < 2) { 
        carp "Must input more than one data point!" unless $self->{hush};
        return 0;
    }
    $self->{numXY} = @$x;
    if (ref $x->[0]) {
        $self->setWeights($y) or return 0;
        $self->{x} = [ ];
        $self->{y} = [ ];
        foreach my $xy (@$x) { 
            push @{$self->{x}}, $xy->[0]; 
            push @{$self->{y}}, $xy->[1]; 
        }
    } else {
        if (@$x != @$y) { 
            carp "Length of x and y arrays must be equal!" unless $self->{hush};
            return 0;
        }
        $self->setWeights($weights) or return 0;
        $self->{x} = [ @$x ];
        $self->{y} = [ @$y ];
    }
    if ($self->{validate}) { 
        unless ($self->validData()) { 
            $self->{x} = $self->{y} = $self->{weights} = $self->{numXY} = undef;
            return 0;
        }
    }
    $self->{gotData} = 1;
    return 1;
}

sub setWeights {
#
# Purpose: Normalize and initialize line fit weighting factors (private method)
# 
    my ($self, $weights) = @_;
    return 1 unless defined $weights;
    if (@$weights != $self->{numXY}) {
        carp "Length of weight array must equal length of data array!"
            unless $self->{hush};
        return 0;
    }
    if ($self->{validate}) { $self->validWeights($weights) or return 0 } 
    my $sumW = my $numNonZero = 0;
    foreach my $weight (@$weights) {
        if ($weight < 0) {
            carp "Weights must be non-negative numbers!" unless $self->{hush};
            return 0;
        }
        $sumW += $weight;
        if ($weight != 0) { ++$numNonZero }
    }
    if ($numNonZero < 2) {
        carp "At least two weights must be nonzero!" unless $self->{hush};
        return 0;
    }
    my $factor = @$weights / $sumW;
    foreach my $weight (@$weights) { $weight *= $factor }
    $self->{weight} = [ @$weights ];
    return 1;
}

sub sigma {
#
# Purpose: Return the estimated homoscedastic standard deviation of the
#          error term
# 
    my $self = shift;
    unless (defined $self->{sigma}) {
        $self->regress() or return;
        $self->{sigma} = $self->{numXY} > 2 ? 
            sqrt($self->sumSqErrors() / ($self->{numXY} - 2)) : 0;
    }
    return $self->{sigma};
}

sub sumSqErrors {
#
# Purpose: Return the sum of the squared errors (private method)
# 
    my $self = shift;
    unless (defined $self->{sumSqErrors}) {
        $self->regress() or return;
        $self->{sumSqErrors} = $self->{sumSqDevY} - $self->{sumSqDevX}
            * $self->{slope} ** 2;
        if ($self->{sumSqErrors} < 0) { $self->{sumSqErrors} = 0 } 
    }
    return $self->{sumSqErrors};
}

sub tStatistics {
#
# Purpose: Return the T statistics
# 
    my $self = shift;
    unless (defined $self->{tStatInt} and defined $self->{tStatSlope}) {
        $self->regress() or return;
        my $biasEstimateInt = $self->sigma() * sqrt($self->{sumXX} 
            / ($self->{sumSqDevX} * $self->{numXY}));
        $self->{tStatInt} = $biasEstimateInt != 0 ?
            $self->{intercept} / $biasEstimateInt : 0;
        my $biasEstimateSlope = $self->sigma() / sqrt($self->{sumSqDevX});
        $self->{tStatSlope} = $biasEstimateSlope != 0 ? 
            $self->{slope} / $biasEstimateSlope : 0;
    }
    return ($self->{tStatInt}, $self->{tStatSlope});
}

sub validData {
#
# Purpose: Verify that the input x-y data are numeric (private method)
# 
    my $self = shift;
    for (my $i = 0; $i < $self->{numXY}; ++$i) {
        if (not defined $self->{x}[$i]) {
            carp "Input x[$i] is not defined" unless $self->{hush};
            return 0;
        }
        if ($self->{x}[$i] !~
            /^([+-]?)(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?$/)
        {
            carp "Input x[$i] is not a number: $self->{x}[$i]" 
                unless $self->{hush};
            return 0;
        }
        if (not defined $self->{y}[$i]) {
            carp "Input y[$i] is not defined" unless $self->{hush};
            return 0;
        }
        if ($self->{y}[$i] !~
            /^([+-]?)(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?$/)
        {
            carp "Input y[$i] is not a number: $self->{y}[$i]"
                unless $self->{hush};
            return 0;
        }
    }
    return 1;
}

sub validWeights {
#
# Purpose: Verify that the input weights are numeric (private method)
# 
    my ($self, $weights) = @_;
    for (my $i = 0; $i < @$weights; ++$i) {
        if (not defined $weights->[$i]) {
            carp "Input weights[$i] is not defined" unless $self->{hush};
            return 0;
        }
        if ($weights->[$i]
            !~ /^([+-]?)(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?$/)
        {
            carp "Input weights[$i] is not a number: $weights->[$i]"
                unless $self->{hush};
            return 0;
        }
    }
    return 1;
}

sub varianceOfEstimates {
#
# Purpose: Return the variances in the estimates of the intercept and slope
# 
    my $self = shift;
    unless (defined $self->{intercept} and defined $self->{slope}) {
        $self->regress() or return;
    }
    my @predictedYs = $self->predictedYs();
    my ($s, $sx, $sxx) = (0, 0, 0);
    if (defined $self->{weight}) {
        for (my $i = 0; $i < $self->{numXY}; ++$i) {
            my $variance = ($predictedYs[$i] - $self->{y}[$i]) ** 2; 
            next if 0 == $variance;
            $s += 1.0 / $variance;
	    $sx += $self->{weight}[$i] * $self->{x}[$i] / $variance;
	    $sxx += $self->{weight}[$i] * $self->{x}[$i] ** 2 / $variance;
        }
    } else {
        for (my $i = 0; $i < $self->{numXY}; ++$i) {
            my $variance = ($predictedYs[$i] - $self->{y}[$i]) ** 2; 
            next if 0 == $variance;
            $s += 1.0 / $variance;
	    $sx += $self->{x}[$i] / $variance;
	    $sxx += $self->{x}[$i] ** 2 / $variance;
        }
    }
    my $denominator = ($s * $sxx - $sx ** 2);
    if (0 == $denominator) {
        return;
    } else {
        return ($sxx / $denominator, $s / $denominator);
    }
}

1;

__END__

=head1 NAME

Statistics::LineFit - Least squares line fit, weighted or unweighted

=head1 SYNOPSIS

 use Statistics::LineFit;
 $lineFit = Statistics::LineFit->new();
 $lineFit->setData (\@xValues, \@yValues) or die "Invalid data";
 ($intercept, $slope) = $lineFit->coefficients();
 defined $intercept or die "Can't fit line if x values are all equal";
 $rSquared = $lineFit->rSquared();
 $meanSquaredError = $lineFit->meanSqError();
 $durbinWatson = $lineFit->durbinWatson();
 $sigma = $lineFit->sigma();
 ($tStatIntercept, $tStatSlope) = $lineFit->tStatistics();
 @predictedYs = $lineFit->predictedYs();
 @residuals = $lineFit->residuals();
 (varianceIntercept, $varianceSlope) = $lineFit->varianceOfEstimates();

=head1 DESCRIPTION

The Statistics::LineFit module does weighted or unweighted least-squares
line fitting to two-dimensional data (y = a + b * x).  (This is also called
linear regression.)  In addition to the slope and y-intercept, the module
can return the square of the correlation coefficient (R squared), the
Durbin-Watson statistic, the mean squared error, sigma, the t statistics,
the variance of the estimates of the slope and y-intercept, 
the predicted y values and the residuals of the y values.  (See the METHODS
section for a description of these statistics.)

The module accepts input data in separate x and y arrays or a single
2-D array (an array of arrayrefs).  The optional weights are input in a
separate array.  The module can optionally verify that the input data and
weights are valid numbers.  If weights are input, the line fit minimizes
the weighted sum of the squared errors and the following statistics are
weighted: the correlation coefficient, the Durbin-Watson statistic, the
mean squared error, sigma and the t statistics.

The module is state-oriented and caches its results.  Once you call the
setData() method, you can call the other methods in any order or call a
method several times without invoking redundant calculations.  After calling
setData(), you can modify the input data or weights without affecting the
module's results.

The decision to use or not use weighting could be made using your a
priori knowledge of the data or using supplemental data.  If the data is
sparse or contains non-random noise, weighting can degrade the solution.
Weighting is a good option if some points are suspect or less relevant (e.g.,
older terms in a time series, points that are known to have more noise).

=head1 ALGORITHM

The least-square line is the line that minimizes the sum of the squares
of the y residuals:

 Minimize SUM((y[i] - (a + b * x[i])) ** 2)

Setting the parial derivatives of a and b to zero yields a solution that
can be expressed in terms of the means, variances and covariances of x and y:

 b = SUM((x[i] - meanX) * (y[i] - meanY)) / SUM((x[i] - meanX) ** 2) 

 a = meanY - b * meanX

Note that a and b are undefined if all the x values are the same.

If you use weights, each term in the above sums is multiplied by the
value of the weight for that index.  The program normalizes the weights
(after copying the input values) so that the sum of the weights equals
the number of points.  This minimizes the differences between the weighted
and unweighted equations.

Statistics::LineFit uses equations that are mathematically equivalent to
the above equations and computationally more efficient.  The module runs
in O(N) (linear time).

=head1 LIMITATIONS

The regression fails if the input x values are all equal or the only unequal
x values have zero weights.  This is an inherent limit to fitting a line of
the form y = a + b * x.  In this case, the module issues an error message
and methods that return statistical values will return undefined values.
You can also use the return value of the regress() method to check the
status of the regression.

As the sum of the squared deviations of the x values approaches zero,
the module's results becomes sensitive to the precision of floating point
operations on the host system.

If the x values are not all the same and the apparent "best fit" line is
vertical, the module will fit a horizontal line.  For example, an input of
(1, 1), (1, 7), (2, 3), (2, 5) returns a slope of zero, an intercept of 4
and an R squared of zero.  This is correct behavior because this line is the
best least-squares fit to the data for the given parameterization 
(y = a + b * x).

On a 32-bit system the results are accurate to about 11 significant digits,
depending on the input data.  Many of the installation tests will fail
on a system with word lengths of 16 bits or fewer.  (You might want to
upgrade your old 80286 IBM PC.)

=head1 EXAMPLES

=head2 Alternate calling sequence:

 use Statistics::LineFit;
 $lineFit = Statistics::LineFit->new();
 $lineFit->setData(\@x, \@y) or die "Invalid regression data\n";
 if (defined $lineFit->rSquared()
     and $lineFit->rSquared() > $threshold) 
 {
     ($intercept, $slope) = $lineFit->coefficients();
     print "Slope: $slope  Y-intercept: $intercept\n";
 }

=head2 Multiple calls with same object, validate input, suppress error messages:

 use Statistics::LineFit;
 $lineFit = Statistics::LineFit->new(1, 1);
 while (1) {
     @xy = read2Dxy();  # User-supplied subroutine
     $lineFit->setData(\@xy);
     ($intercept, $slope) = $lineFit->coefficients();
     if (defined $intercept) {
         print "Slope: $slope  Y-intercept: $intercept\n";
     } 
 }

=head1 METHODS

The module is state-oriented and caches its results.  Once you call the
setData() method, you can call the other methods in any order or call
a method several times without invoking redundant calculations.

The regression fails if the x values are all the same.  In this case,
the module issues an error message and methods that return statistical
values will return undefined values.  You can also use the return value 
of the regress() method to check the status of the regression.

=head2 new() - create a new Statistics::LineFit object

 $lineFit = Statistics::LineFit->new();
 $lineFit = Statistics::LineFit->new($validate);
 $lineFit = Statistics::LineFit->new($validate, $hush);

 $validate = 1 -> Verify input data is numeric (slower execution)
             0 -> Don't verify input data (default, faster execution)
 $hush = 1 -> Suppress error messages
       = 0 -> Enable error messages (default)

=head2 coefficients() - Return the slope and y intercept

 ($intercept, $slope) = $lineFit->coefficients();

The returned list is undefined if the regression fails.

=head2 durbinWatson() - Return the Durbin-Watson statistic

 $durbinWatson = $lineFit->durbinWatson();

The Durbin-Watson test is a test for first-order autocorrelation in
the residuals of a time series regression. The Durbin-Watson statistic
has a range of 0 to 4; a value of 2 indicates there is no
autocorrelation.

The return value is undefined if the regression fails.  If weights are
input, the return value is the weighted Durbin-Watson statistic.

=head2 meanSqError() - Return the mean squared error

 $meanSquaredError = $lineFit->meanSqError();

The return value is undefined if the regression fails.  If weights are
input, the return value is the weighted mean squared error. 

=head2 predictedYs() - Return the predicted y values

 @predictedYs = $lineFit->predictedYs();

The returned list is undefined if the regression fails.

=head2 regress() - Do the least squares line fit (if not already done)

 $lineFit->regress() or die "Regression failed"

You don't need to call this method because it is invoked by the other
methods as needed.  After you call setData(), you can call regress()
at any time to get the status of the regression for the current data.

=head2 residuals() - Return predicted y values minus input y values

 @residuals = $lineFit->residuals();

The returned list is undefined if the regression fails.

=head2 rSquared() - Return the square of the correlation coefficient

 $rSquared = $lineFit->rSquared();

R squared, also called the square of the Pearson product-moment correlation
coefficient, is a measure of goodness-of-fit.  It is the fraction of the
variation in Y that can be attributed to the variation in X.  A perfect fit
will have an R squared of 1; fitting a line to the vertices of a
regular polygon will yield an R squared of zero.  Graphical displays of data
with an R squared of less than about 0.1 do not show a visible linear trend.

The return value is undefined if the regression fails.  If weights are 
input, the return value is the weighted correlation coefficient.

=head2 setData() - Initialize (x,y) values and optional weights

 $lineFit->setData(\@x, \@y) or die "Invalid regression data";
 $lineFit->setData(\@x, \@y, \@weights) or die "Invalid regression data";
 $lineFit->setData(\@xy) or die "Invalid regression data";
 $lineFit->setData(\@xy, \@weights) or die "Invalid regression data";

@xy is an array of arrayrefs; x values are $xy[$i][0], y values are
$xy[$i][1].  (The module does not access any indices greater than $xy[$i][1],
so the arrayrefs can point to arrays that are longer than two elements.)
The method identifies the difference between the first and fourth calling
signatures by examining the first argument.

The optional weights array must be the same length as the data array(s).
The weights must be non-negative numbers; at least two of the weights
must be nonzero.  Only the relative size of the weights is significant:
the program normalizes the weights (after copying the input values) so
that the sum of the weights equals the number of points.  If you want to
do multiple line fits using the same weights, the weights must be passed
to each call to setData().

The method will return zero if the array lengths don't match, there are
less than two data points, any weights are negative or less than two of
the weights are nonzero. If the new() method was called with validate = 1,
the method will also verify that the data and weights are valid numbers.
Once you successfully call setData(), the next call to any method other than
new() or setData() invokes the regression.  You can modify the input data
or weights after calling setData() without affecting the module's results.

=head2 sigma() - Return the standard error of the estimate

$sigma = $lineFit->sigma();

Sigma is an estimate of the homoscedastic standard deviation of the
error.  Sigma is also known as the standard error of the estimate.

The return value is undefined if the regression fails.  If weights are
input, the return value is the weighted standard error.

=head2 tStatistics() - Return the t statistics

 (tStatIntercept, $tStatSlope) = $lineFit->tStatistics();

The t statistic, also called the t ratio or Wald statistic, is used to
accept or reject a hypothesis using a table of cutoff values computed from
the t distribution.  The t-statistic suggests that the estimated value is
(reasonable, too small, too large) when the t-statistic is (close to zero,
large and positive, large and negative).

The returned list is undefined if the regression fails.  If weights 
are input, the returned values are the weighted t statistics.

=head2 varianceOfEstimates() - Return variances of estimates of intercept, slope

 (varianceIntercept, $varianceSlope) = $lineFit->varianceOfEstimates();

Assuming the data are noisy or inaccurate, the intercept and slope returned
by the coefficients() method are only estimates of the true intercept and 
slope.  The varianceofEstimate() method returns the variances of the 
estimates of the intercept and slope, respectively.  See Numerical Recipes
in C, section 15.2 (Fitting Data to a Straight Line), equation 15.2.9.

The returned list is undefined if the regression fails.  If weights 
are input, the returned values are the weighted variances.

=head1 SEE ALSO

 Mendenhall, W., and Sincich, T.L., 2003, A Second Course in Statistics:
   Regression Analysis, 6th ed., Prentice Hall.
 Press, W. H., Flannery, B. P., Teukolsky, S. A., Vetterling, W. T., 1992,
   Numerical Recipes in C : The Art of Scientific Computing, 2nd ed., 
   Cambridge University Press.
 The man page for perl(1).
 The CPAN modules Statistics::OLS, Statistics::GaussHelmert and 
   Statistics::Regression.

Statistics::LineFit is simpler to use than Statistics::GaussHelmert or
Statistics::Regression.  Statistics::LineFit was inspired by and borrows some
ideas from the venerable Statistics::OLS module.  

The significant differences
between Statistics::LineFit and Statistics::OLS (version 0.07) are:

=over 4

=item B<Statistics::LineFit is more robust.>

Statistics::OLS returns incorrect results for certain input datasets. 
Statistics::OLS does not deep copy its input arrays, which can lead
to subtle bugs.  The Statistics::OLS installation test has only one
test and does not verify that the regression returns correct results.
In contrast, Statistics::LineFit has over 200 installation tests that use
various datasets/calling sequences to verify the accuracy of the
regression to within 1.0e-10.

=item B<Statistics::LineFit is faster.>

For a sequence of calls to new(), setData(\@x, \@y) and regress(),
Statistics::LineFit is faster than Statistics::OLS by factors of 2.0, 1.6
and 2.4 for array lengths of 5, 100 and 10000, respectively.

=item B<Statistics::LineFit can do weighted or unweighted regression.>

Statistics::OLS lacks this option.

=item B<Statistics::LineFit has a better interface.>

Once you call the Statistics::LineFit::setData() method, you can call the
other methods in any order and call methods multiple times without invoking
redundant calculations.  Statistics::LineFit lets you enable or disable
data verification or error messages.

=item B<Statistics::LineFit has better code and documentation.>

The code in Statistics::LineFit is more readable, more object oriented and
more compliant with Perl coding standards than the code in Statistics::OLS.
The documentation for Statistics::LineFit is more detailed and complete.

=back

=head1 AUTHOR

Richard Anderson, cpan(AT)richardanderson(DOT)org,
http://www.richardanderson.org

=head1 LICENSE

This program is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

The full text of the license can be found in the LICENSE file included in
the distribution and available in the CPAN listing for Statistics::LineFit
(see www.cpan.org or search.cpan.org).

=head1 DISCLAIMER

To the maximum extent permitted by applicable law, the author of this
module disclaims all warranties, either express or implied, including
but not limited to implied warranties of merchantability and fitness for
a particular purpose, with regard to the software and the accompanying
documentation.

=cut

1;
