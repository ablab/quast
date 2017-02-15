/* GNUPLOT - qt_conversion.cpp */

/*[
 * Copyright 2009   Jérôme Lodewyck
 *
 * Permission to use, copy, and distribute this software and its
 * documentation for any purpose with or without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.
 *
 * Permission to modify the software is granted, but not the right to
 * distribute the complete modified source code.  Modifications are to
 * be distributed as patches to the released version.  Permission to
 * distribute binaries produced by compiling modified sources is granted,
 * provided you
 *   1. distribute the corresponding source modifications from the
 *    released version in the form of a patch file along with the binaries,
 *   2. add special version identification to distinguish your version
 *    in addition to the base release version number,
 *   3. provide your name and address as the primary contact for the
 *    support of your modified version, and
 *   4. retain our contact information in regard to use of the base
 *    software.
 * Permission to distribute the released version of the source code along
 * with corresponding source modifications in the form of a patch file is
 * granted with same provisions 2 through 4 for binary distributions.
 *
 * This software is provided "as is" without express or implied warranty
 * to the extent permitted by applicable law.
 *
 *
 * Alternatively, the contents of this file may be used under the terms of the
 * GNU General Public License Version 2 or later (the "GPL"), in which case the
 * provisions of GPL are applicable instead of those above. If you wish to allow
 * use of your version of this file only under the terms of the GPL and not
 * to allow others to use your version of this file under the above gnuplot
 * license, indicate your decision by deleting the provisions above and replace
 * them with the notice and other provisions required by the GPL. If you do not
 * delete the provisions above, a recipient may use your version of this file
 * under either the GPL or the gnuplot license.
]*/

static QColor qt_colorList[12] =
{
	Qt::white,
	Qt::black,
	Qt::gray,
	Qt::red,
	Qt::green,
	Qt::blue,
	Qt::magenta,
	Qt::cyan,
	Qt::yellow,
	Qt::black,
	QColor(255, 76, 0), // Orange
	Qt::gray
};

QTextCodec* qt_encodingToCodec(set_encoding_id encoding)
{
	switch (encoding)
	{
	case S_ENC_DEFAULT    : return QTextCodec::codecForLocale();
	case S_ENC_ISO8859_1  : return QTextCodec::codecForMib(   4);
	case S_ENC_ISO8859_2  : return QTextCodec::codecForMib(   5);
	case S_ENC_ISO8859_9  : return QTextCodec::codecForMib(  12);
	case S_ENC_ISO8859_15 : return QTextCodec::codecForMib( 111);
	case S_ENC_CP437      : return QTextCodec::codecForMib(2011);
	case S_ENC_CP850      : return QTextCodec::codecForMib(2009);
	case S_ENC_CP852      : return QTextCodec::codecForMib(2010);
	case S_ENC_CP950      : return QTextCodec::codecForMib(2026); // Note: CP950 has no IANA number. 2026 is for Big5, which is close to CP950
	case S_ENC_CP1250     : return QTextCodec::codecForMib(2250);
	case S_ENC_CP1251     : return QTextCodec::codecForMib(2251);
	case S_ENC_CP1252     : return QTextCodec::codecForMib(2252);
	case S_ENC_CP1254     : return QTextCodec::codecForMib(2254);
	case S_ENC_KOI8_R     : return QTextCodec::codecForMib(2084);
	case S_ENC_KOI8_U     : return QTextCodec::codecForMib(2088);
	case S_ENC_SJIS       : return QTextCodec::codecForMib(  17);
	case S_ENC_UTF8       : return QTextCodec::codecForMib( 106);
	default               : return QTextCodec::codecForLocale();
	}
}

QImage qt_imageToQImage(int M, int N, coordval* image, t_imagecolor color_mode)
{
	QImage qimage(QSize(M, N), QImage::Format_ARGB32_Premultiplied);

	rgb_color rgb1;
	rgb255_color rgb255;

	// TrueColor 24-bit color mode
	if (color_mode == IC_RGB)
		for (int n = 0; n < N; n++)
		{
			QRgb* line = (QRgb*)(qimage.scanLine(n));
			for (int m = 0; m < M; m++)
			{
				rgb1.r = *image++;
				rgb1.g = *image++;
				rgb1.b = *image++;
				rgb255_from_rgb1(rgb1, &rgb255);
				*line++ = qRgb(rgb255.r, rgb255.g, rgb255.b);
			}
		}
	else if (color_mode == IC_RGBA)
		for (int n = 0; n < N; n++)
		{
			QRgb* line = (QRgb*)(qimage.scanLine(n));
			for (int m = 0; m < M; m++)
			{
				unsigned char alpha255 = *(image + 3);
				float alpha1 = float(alpha255 / 255.);
				rgb1.r = alpha1 * (*image++);
				rgb1.g = alpha1 * (*image++);
				rgb1.b = alpha1 * (*image++);
				image++;
				rgb255_from_rgb1(rgb1, &rgb255);
				*line++ = qRgba(rgb255.r, rgb255.g, rgb255.b, alpha255);
			}
		}
	// Palette color lookup from gray value
	else
		for (int n = 0; n < N; n++)
		{
			QRgb* line = (QRgb*)(qimage.scanLine(n));
			for (int m = 0; m < M; m++)
			{
				if (*image != *image)
				{
					image++;
					*line++ = 0x00000000;
				}
				else
				{
					rgb255maxcolors_from_gray(*image++, &rgb255);
					*line++ = qRgb(rgb255.r, rgb255.g, rgb255.b);
				}
			}
		}

	return qimage;
}

// Generated from http://unicode.org/Public/MAPPINGS/VENDORS/ADOBE/symbol.txt
QChar qt_symbolToUnicode(int c)
{
	switch (c)
	{
	case 0x22: return QChar(0x2200); // FOR ALL	// universal
	case 0x24: return QChar(0x2203); // THERE EXISTS	// existential
	case 0x27: return QChar(0x220B); // CONTAINS AS MEMBER	// suchthat
	case 0x2A: return QChar(0x2217); // ASTERISK OPERATOR	// asteriskmath
	case 0x2D: return QChar(0x2212); // MINUS SIGN	// minus
	case 0x40: return QChar(0x2245); // APPROXIMATELY EQUAL TO	// congruent
	case 0x41: return QChar(0x0391); // GREEK CAPITAL LETTER ALPHA	// Alpha
	case 0x42: return QChar(0x0392); // GREEK CAPITAL LETTER BETA	// Beta
	case 0x43: return QChar(0x03A7); // GREEK CAPITAL LETTER CHI	// Chi
	case 0x44: return QChar(0x0394); // GREEK CAPITAL LETTER DELTA	// Delta
	case 0x45: return QChar(0x0395); // GREEK CAPITAL LETTER EPSILON	// Epsilon
	case 0x46: return QChar(0x03A6); // GREEK CAPITAL LETTER PHI	// Phi
	case 0x47: return QChar(0x0393); // GREEK CAPITAL LETTER GAMMA	// Gamma
	case 0x48: return QChar(0x0397); // GREEK CAPITAL LETTER ETA	// Eta
	case 0x49: return QChar(0x0399); // GREEK CAPITAL LETTER IOTA	// Iota
	case 0x4A: return QChar(0x03D1); // GREEK THETA SYMBOL	// theta1
	case 0x4B: return QChar(0x039A); // GREEK CAPITAL LETTER KAPPA	// Kappa
	case 0x4C: return QChar(0x039B); // GREEK CAPITAL LETTER LAMDA	// Lambda
	case 0x4D: return QChar(0x039C); // GREEK CAPITAL LETTER MU	// Mu
	case 0x4E: return QChar(0x039D); // GREEK CAPITAL LETTER NU	// Nu
	case 0x4F: return QChar(0x039F); // GREEK CAPITAL LETTER OMICRON	// Omicron
	case 0x50: return QChar(0x03A0); // GREEK CAPITAL LETTER PI	// Pi
	case 0x51: return QChar(0x0398); // GREEK CAPITAL LETTER THETA	// Theta
	case 0x52: return QChar(0x03A1); // GREEK CAPITAL LETTER RHO	// Rho
	case 0x53: return QChar(0x03A3); // GREEK CAPITAL LETTER SIGMA	// Sigma
	case 0x54: return QChar(0x03A4); // GREEK CAPITAL LETTER TAU	// Tau
	case 0x55: return QChar(0x03A5); // GREEK CAPITAL LETTER UPSILON	// Upsilon
	case 0x56: return QChar(0x03C2); // GREEK SMALL LETTER FINAL SIGMA	// sigma1
	case 0x57: return QChar(0x03A9); // GREEK CAPITAL LETTER OMEGA	// Omega
	case 0x58: return QChar(0x039E); // GREEK CAPITAL LETTER XI	// Xi
	case 0x59: return QChar(0x03A8); // GREEK CAPITAL LETTER PSI	// Psi
	case 0x5A: return QChar(0x0396); // GREEK CAPITAL LETTER ZETA	// Zeta
	case 0x5C: return QChar(0x2234); // THEREFORE	// therefore
	case 0x5E: return QChar(0x22A5); // UP TACK	// perpendicular
	case 0x60: return QChar(0xF8E5); // RADICAL EXTENDER	// radicalex (CUS)
	case 0x61: return QChar(0x03B1); // GREEK SMALL LETTER ALPHA	// alpha
	case 0x62: return QChar(0x03B2); // GREEK SMALL LETTER BETA	// beta
	case 0x63: return QChar(0x03C7); // GREEK SMALL LETTER CHI	// chi
	case 0x64: return QChar(0x03B4); // GREEK SMALL LETTER DELTA	// delta
	case 0x65: return QChar(0x03B5); // GREEK SMALL LETTER EPSILON	// epsilon
	case 0x66: return QChar(0x03C6); // GREEK SMALL LETTER PHI	// phi
	case 0x67: return QChar(0x03B3); // GREEK SMALL LETTER GAMMA	// gamma
	case 0x68: return QChar(0x03B7); // GREEK SMALL LETTER ETA	// eta
	case 0x69: return QChar(0x03B9); // GREEK SMALL LETTER IOTA	// iota
	case 0x6A: return QChar(0x03D5); // GREEK PHI SYMBOL	// phi1
	case 0x6B: return QChar(0x03BA); // GREEK SMALL LETTER KAPPA	// kappa
	case 0x6C: return QChar(0x03BB); // GREEK SMALL LETTER LAMDA	// lambda
	case 0x6D: return QChar(0x03BC); // GREEK SMALL LETTER MU	// mu
	case 0x6E: return QChar(0x03BD); // GREEK SMALL LETTER NU	// nu
	case 0x6F: return QChar(0x03BF); // GREEK SMALL LETTER OMICRON	// omicron
	case 0x70: return QChar(0x03C0); // GREEK SMALL LETTER PI	// pi
	case 0x71: return QChar(0x03B8); // GREEK SMALL LETTER THETA	// theta
	case 0x72: return QChar(0x03C1); // GREEK SMALL LETTER RHO	// rho
	case 0x73: return QChar(0x03C3); // GREEK SMALL LETTER SIGMA	// sigma
	case 0x74: return QChar(0x03C4); // GREEK SMALL LETTER TAU	// tau
	case 0x75: return QChar(0x03C5); // GREEK SMALL LETTER UPSILON	// upsilon
	case 0x76: return QChar(0x03D6); // GREEK PI SYMBOL	// omega1
	case 0x77: return QChar(0x03C9); // GREEK SMALL LETTER OMEGA	// omega
	case 0x78: return QChar(0x03BE); // GREEK SMALL LETTER XI	// xi
	case 0x79: return QChar(0x03C8); // GREEK SMALL LETTER PSI	// psi
	case 0x7A: return QChar(0x03B6); // GREEK SMALL LETTER ZETA	// zeta
	case 0x7E: return QChar(0x223C); // TILDE OPERATOR	// similar
	case 0xA0: return QChar(0x20AC); // EURO SIGN	// Euro
	case 0xA1: return QChar(0x03D2); // GREEK UPSILON WITH HOOK SYMBOL	// Upsilon1
	case 0xA2: return QChar(0x2032); // PRIME	// minute
	case 0xA3: return QChar(0x2264); // LESS-THAN OR EQUAL TO	// lessequal
	case 0xA4: return QChar(0x2044); // FRACTION SLASH	// fraction
	case 0xA5: return QChar(0x221E); // INFINITY	// infinity
	case 0xA6: return QChar(0x0192); // LATIN SMALL LETTER F WITH HOOK	// florin
	case 0xA7: return QChar(0x2663); // BLACK CLUB SUIT	// club
	case 0xA8: return QChar(0x2666); // BLACK DIAMOND SUIT	// diamond
	case 0xA9: return QChar(0x2665); // BLACK HEART SUIT	// heart
	case 0xAA: return QChar(0x2660); // BLACK SPADE SUIT	// spade
	case 0xAB: return QChar(0x2194); // LEFT RIGHT ARROW	// arrowboth
	case 0xAC: return QChar(0x2190); // LEFTWARDS ARROW	// arrowleft
	case 0xAD: return QChar(0x2191); // UPWARDS ARROW	// arrowup
	case 0xAE: return QChar(0x2192); // RIGHTWARDS ARROW	// arrowright
	case 0xAF: return QChar(0x2193); // DOWNWARDS ARROW	// arrowdown
	case 0xB2: return QChar(0x2033); // DOUBLE PRIME	// second
	case 0xB3: return QChar(0x2265); // GREATER-THAN OR EQUAL TO	// greaterequal
	case 0xB4: return QChar(0x00D7); // MULTIPLICATION SIGN	// multiply
	case 0xB5: return QChar(0x221D); // PROPORTIONAL TO	// proportional
	case 0xB6: return QChar(0x2202); // PARTIAL DIFFERENTIAL	// partialdiff
	case 0xB7: return QChar(0x2022); // BULLET	// bullet
	case 0xB8: return QChar(0x00F7); // DIVISION SIGN	// divide
	case 0xB9: return QChar(0x2260); // NOT EQUAL TO	// notequal
	case 0xBA: return QChar(0x2261); // IDENTICAL TO	// equivalence
	case 0xBB: return QChar(0x2248); // ALMOST EQUAL TO	// approxequal
	case 0xBC: return QChar(0x2026); // HORIZONTAL ELLIPSIS	// ellipsis
	case 0xBD: return QChar(0xF8E6); // VERTICAL ARROW EXTENDER	// arrowvertex (CUS)
	case 0xBE: return QChar(0xF8E7); // HORIZONTAL ARROW EXTENDER	// arrowhorizex (CUS)
	case 0xBF: return QChar(0x21B5); // DOWNWARDS ARROW WITH CORNER LEFTWARDS	// carriagereturn
	case 0xC0: return QChar(0x2135); // ALEF SYMBOL	// aleph
	case 0xC1: return QChar(0x2111); // BLACK-LETTER CAPITAL I	// Ifraktur
	case 0xC2: return QChar(0x211C); // BLACK-LETTER CAPITAL R	// Rfraktur
	case 0xC3: return QChar(0x2118); // SCRIPT CAPITAL P	// weierstrass
	case 0xC4: return QChar(0x2297); // CIRCLED TIMES	// circlemultiply
	case 0xC5: return QChar(0x2295); // CIRCLED PLUS	// circleplus
	case 0xC6: return QChar(0x2205); // EMPTY SET	// emptyset
	case 0xC7: return QChar(0x2229); // INTERSECTION	// intersection
	case 0xC8: return QChar(0x222A); // UNION	// union
	case 0xC9: return QChar(0x2283); // SUPERSET OF	// propersuperset
	case 0xCA: return QChar(0x2287); // SUPERSET OF OR EQUAL TO	// reflexsuperset
	case 0xCB: return QChar(0x2284); // NOT A SUBSET OF	// notsubset
	case 0xCC: return QChar(0x2282); // SUBSET OF	// propersubset
	case 0xCD: return QChar(0x2286); // SUBSET OF OR EQUAL TO	// reflexsubset
	case 0xCE: return QChar(0x2208); // ELEMENT OF	// element
	case 0xCF: return QChar(0x2209); // NOT AN ELEMENT OF	// notelement
	case 0xD0: return QChar(0x2220); // ANGLE	// angle
	case 0xD1: return QChar(0x2207); // NABLA	// gradient
	case 0xD2: return QChar(0xF6DA); // REGISTERED SIGN SERIF	// registerserif (CUS)
	case 0xD3: return QChar(0xF6D9); // COPYRIGHT SIGN SERIF	// copyrightserif (CUS)
	case 0xD4: return QChar(0xF6DB); // TRADE MARK SIGN SERIF	// trademarkserif (CUS)
	case 0xD5: return QChar(0x220F); // N-ARY PRODUCT	// product
	case 0xD6: return QChar(0x221A); // SQUARE ROOT	// radical
	case 0xD7: return QChar(0x22C5); // DOT OPERATOR	// dotmath
	case 0xD8: return QChar(0x00AC); // NOT SIGN	// logicalnot
	case 0xD9: return QChar(0x2227); // LOGICAL AND	// logicaland
	case 0xDA: return QChar(0x2228); // LOGICAL OR	// logicalor
	case 0xDB: return QChar(0x21D4); // LEFT RIGHT DOUBLE ARROW	// arrowdblboth
	case 0xDC: return QChar(0x21D0); // LEFTWARDS DOUBLE ARROW	// arrowdblleft
	case 0xDD: return QChar(0x21D1); // UPWARDS DOUBLE ARROW	// arrowdblup
	case 0xDE: return QChar(0x21D2); // RIGHTWARDS DOUBLE ARROW	// arrowdblright
	case 0xDF: return QChar(0x21D3); // DOWNWARDS DOUBLE ARROW	// arrowdbldown
	case 0xE0: return QChar(0x25CA); // LOZENGE	// lozenge
	case 0xE1: return QChar(0x2329); // LEFT-POINTING ANGLE BRACKET	// angleleft
	case 0xE2: return QChar(0xF8E8); // REGISTERED SIGN SANS SERIF	// registersans (CUS)
	case 0xE3: return QChar(0xF8E9); // COPYRIGHT SIGN SANS SERIF	// copyrightsans (CUS)
	case 0xE4: return QChar(0xF8EA); // TRADE MARK SIGN SANS SERIF	// trademarksans (CUS)
	case 0xE5: return QChar(0x2211); // N-ARY SUMMATION	// summation
	case 0xE6: return QChar(0xF8EB); // LEFT PAREN TOP	// parenlefttp (CUS)
	case 0xE7: return QChar(0xF8EC); // LEFT PAREN EXTENDER	// parenleftex (CUS)
	case 0xE8: return QChar(0xF8ED); // LEFT PAREN BOTTOM	// parenleftbt (CUS)
	case 0xE9: return QChar(0xF8EE); // LEFT SQUARE BRACKET TOP	// bracketlefttp (CUS)
	case 0xEA: return QChar(0xF8EF); // LEFT SQUARE BRACKET EXTENDER	// bracketleftex (CUS)
	case 0xEB: return QChar(0xF8F0); // LEFT SQUARE BRACKET BOTTOM	// bracketleftbt (CUS)
	case 0xEC: return QChar(0xF8F1); // LEFT CURLY BRACKET TOP	// bracelefttp (CUS)
	case 0xED: return QChar(0xF8F2); // LEFT CURLY BRACKET MID	// braceleftmid (CUS)
	case 0xEE: return QChar(0xF8F3); // LEFT CURLY BRACKET BOTTOM	// braceleftbt (CUS)
	case 0xEF: return QChar(0xF8F4); // CURLY BRACKET EXTENDER	// braceex (CUS)
	case 0xF1: return QChar(0x232A); // RIGHT-POINTING ANGLE BRACKET	// angleright
	case 0xF2: return QChar(0x222B); // INTEGRAL	// integral
	case 0xF3: return QChar(0x2320); // TOP HALF INTEGRAL	// integraltp
	case 0xF4: return QChar(0xF8F5); // INTEGRAL EXTENDER	// integralex (CUS)
	case 0xF5: return QChar(0x2321); // BOTTOM HALF INTEGRAL	// integralbt
	case 0xF6: return QChar(0xF8F6); // RIGHT PAREN TOP	// parenrighttp (CUS)
	case 0xF7: return QChar(0xF8F7); // RIGHT PAREN EXTENDER	// parenrightex (CUS)
	case 0xF8: return QChar(0xF8F8); // RIGHT PAREN BOTTOM	// parenrightbt (CUS)
	case 0xF9: return QChar(0xF8F9); // RIGHT SQUARE BRACKET TOP	// bracketrighttp (CUS)
	case 0xFA: return QChar(0xF8FA); // RIGHT SQUARE BRACKET EXTENDER	// bracketrightex (CUS)
	case 0xFB: return QChar(0xF8FB); // RIGHT SQUARE BRACKET BOTTOM	// bracketrightbt (CUS)
	case 0xFC: return QChar(0xF8FC); // RIGHT CURLY BRACKET TOP	// bracerighttp (CUS)
	case 0xFD: return QChar(0xF8FD); // RIGHT CURLY BRACKET MID	// bracerightmid (CUS)
	case 0xFE: return QChar(0xF8FE); // RIGHT CURLY BRACKET BOTTOM	// bracerightbt (CUS)
	default  : return QChar(c);
	}
}
