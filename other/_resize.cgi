#!/usr/bin/perl
use strict;
use warnings;
use CGI;

use LWP::UserAgent;
use Image::Magick;

my $im = Image::Magick->new;

my $q = new CGI;
my $imgurl =  $q->param( '_u' );
# $imgurl = 'https://i.scdn.co/image/ab67616d00001e02f273798606bb1143457c3875';
my $ua = LWP::UserAgent->new;
my $req = HTTP::Request->new(GET => "$imgurl");
my $res = $ua->request($req);
my $get_img = $res->content;

$im->BlobToImage($get_img);

my $resized = $im->Clone();
$resized->Resize(geometry => "128x128");
# 真四角ではないデータがあるためクロッピングする
$resized->Extent(width=>128, height=>128, background=>'black');

my $img_size;
my $file = "./_tmp.jpg";

$resized->Write("jpeg:$file");
open my $fh, '<', $file;
binmode $fh;
my $tmp_data = do { local $/; <$fh> };
close $fh;
$img_size = -s $file;
unlink $file;


print "Content-Length: ",$img_size,"\n";
print "Content-type:image/jpeg\n\n";
# print "Content-type: text/plain\n\n";
binmode(STDOUT);
$resized->Write("jpeg:-");

# print "$img_size\n";
