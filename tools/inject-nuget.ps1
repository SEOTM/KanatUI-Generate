# ================================
# 脚本: inject-nuget.ps1
# 描述: 向 cmake 生成的 out\GenUI.vcxproj 注入 NuGet PackageReference
#       (GenUI.Deps.Native)。每次重新 cmake configure 后需重跑。
# 用法: 在仓库根执行 pwsh -File tools\inject-nuget.ps1 [-Version 1.0.0]
# ================================
param(
    [string]$Root      = 'GenUI',
    [string]$PackageId = 'GenUI.Deps.Native',
    [string]$Version   = '1.0.0'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vcx    = Join-Path $repoRoot "out\$Root.vcxproj"

if (-not (Test-Path $vcx)) {
    throw "未找到生成的工程文件: $vcx (请先执行 cmake configure)"
}
$xml = [System.IO.File]::ReadAllText($vcx)
if ($xml.Contains($PackageId)) {
    Write-Output "已包含 $PackageId, 无需重复注入"
    exit 0
}
$itemGroup = @"

  <ItemGroup>
    <PackageReference Include="$PackageId">
      <Version>$Version</Version>
    </PackageReference>
  </ItemGroup>
"@
$idx = $xml.LastIndexOf('</Project>')
if ($idx -lt 0) { throw "工程文件缺少 </Project> 结束标记: $vcx" }
$xml = $xml.Substring(0, $idx) + $itemGroup + $xml.Substring($idx)
[System.IO.File]::WriteAllText($vcx, $xml, (New-Object System.Text.UTF8Encoding($false)))
Write-Output "已注入 NuGet 包引用 $PackageId $Version -> $vcx"
