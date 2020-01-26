param(
	[string]$port = "7070",
	[string]$path = "./",
	[string]$depth = "1"
)
$compareFile = "compare_res"
$outDir = "out"
$excludes = 'test[\\/]'
Write-Output "Testing files in $path
Recursion limited to $depth
Port $port"
if (Test-Path $compareFile) {
	Clear-Content $compareFile
}
if (!(Test-Path $outDir)) {
	mkdir $outDir *>$null
}
if (Test-Path $outDir) {
	Remove-Item -r "$outDir/*"
}
$result = $(Get-ChildItem -Recurse -Depth $depth -Path $path | 
	Where-Object { $_.FullName -notmatch ($excludes) } | 
	ForEach-Object {
		Write-Output "porco dio"
		echo $_.FullName
		$relPath = ($_ | Resolve-Path -Relative).replace($path, "")
		$isDir = $(Test-Path -Path $_.fullname -PathType Container)
		if ($isDir) {
			$relPath = "$relPath/"
		}
		$out = $relPath -replace '[\\/]', '_'
		mycurl gopher://localhost:$port//$relPath --output $outDir/$out *>$null 
		if (!$isDir) {
			$comp = (fc.exe /b $relPath $outDir\$out 2>null) | Out-String
			Write-Output $comp | Out-File $compareFile -Append
			return $comp -notmatch '[A-Z\d]{8}: ([A-Z\d]{2} ?){2}'
		}
	}) -notcontains $False
if ($result) {
	Write-Output "All tests successfull!"
}
else {
	Write-Output "Some tests failed"
}
Write-Output "Check comparison results in $compareFile"