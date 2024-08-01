<?php

// パラメータからURLを取得
$url = isset($_GET['_u']) ? $_GET['_u'] : null;

if (!$url) {
    http_response_code(400);
    echo "URLパラメータが必要です。";
    exit;
}

// 画像をダウンロード
$image = file_get_contents($url);
if ($image === false) {
    http_response_code(500);
    echo "画像のダウンロードに失敗しました。";
    exit;
}

// 画像リソースを作成
$sourceImage = imagecreatefromstring($image);
if ($sourceImage === false) {
    http_response_code(500);
    echo "画像の作成に失敗しました。";
    exit;
}

// 画像のサイズを取得
$width = imagesx($sourceImage);
$height = imagesy($sourceImage);

// 新しい画像のサイズ
$newWidth = 128;
$newHeight = 128;

// 新しい画像リソースを作成
$targetImage = imagecreatetruecolor($newWidth, $newHeight);

// 黒で塗りつぶす
$black = imagecolorallocate($targetImage, 0, 0, 0);
imagefill($targetImage, 0, 0, $black);

// 元の画像の比率を維持して中央に配置
$srcRatio = $width / $height;
$targetRatio = $newWidth / $newHeight;

if ($srcRatio > $targetRatio) {
    $resizeWidth = $newWidth;
    $resizeHeight = $newWidth / $srcRatio;
} else {
    $resizeHeight = $newHeight;
    $resizeWidth = $newHeight * $srcRatio;
}

$dstX = ($newWidth - $resizeWidth) / 2;
$dstY = ($newHeight - $resizeHeight) / 2;

imagecopyresampled($targetImage, $sourceImage, $dstX, $dstY, 0, 0, $resizeWidth, $resizeHeight, $width, $height);

// バッファリングを開始
ob_start();
imagejpeg($targetImage);
$imageData = ob_get_clean();

// Content-Lengthを設定して出力
header('Content-Type: image/jpeg');
header('Content-Length: ' . strlen($imageData));
echo $imageData;

// 画像リソースを解放
imagedestroy($sourceImage);
imagedestroy($targetImage);
?>

