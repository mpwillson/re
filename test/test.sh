#!/bin/sh

echo "[Simple sequence of characters: abc]"
./ret abc <<EOF
abc
not
EOF
echo "[Simple alternate characters: a|b|c]"
./ret "a|b|c" <<EOF
a
b
c
x
EOF
echo "[Alternate words: this|that|theother]"
./ret "this|that|theother" <<EOF
this
that
theother
xx theother xxx
xx
EOF
echo "[Alternate words inside parens: ((this|that|theother))]"
./ret "((this|that|theother))" <<EOF
this
that
theother
xx theother xxx
xx
EOF
echo "[Alternate words inside parens: ((this)|((that))|(theother))]"
./ret "((this)|((that))|(theother))" <<EOF
this
that
theother
xx theother xxx
xx
EOF
echo "[Simple closure- abc*]"
./ret "abc*" <<EOF
ab
abc
abcccccccccccccccccc
a
EOF
echo "[Expression closure- z(abc)*z]"
./ret "z(abc)*z" <<EOF
zz
zabcz
zabcabcabcz
zabz
EOF
echo "[Combination of alternates and closures: ((a|b)|(c|d)kk*)*z]"
./ret "((a|b)|(c|d)kk*)*z" <<EOF
z
az
aaaaz
ababz
ckz
dkz
dkkkkkkkkkz
ckkkkkkkkkz
d
EOF
echo "[Two character alternates: th(ei|ie)r]"
./ret "th(ei|ie)r" <<EOF
their
thier
padding their padding
thr
EOF
echo "[With added closure: th(ei|ie)*r]"
./ret "th(ei|ie)*r" <<EOF
their
theieieieir
thieeiieeir
thr
xxx
EOF
echo "[More alternate closures: z(aa*|b(b)*)z]"
./ret "z(aa*|b(b)*)z" <<EOF
zaz
zaaaaaaaaaaaz
zbz
zbbbbbbbbbbbz
zz
EOF
echo "[More alternate closures: z(aa*|b(b)*|ccc)*z]"
./ret "z(aa*|b(b)*|ccc)*z" <<EOF
zaz
zaaaaaaaaaaaz
zbz
zbbbbbbbbbbbz
zz
zcccz
kz
EOF
echo "[Alternates with closure and shared starting chars: (a*b|ac)d]"
./ret "(a*b|ac)d" <<EOF
abd
acd
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabd
ddddddd
EOF
# Testing anchors

echo "[Anchors: ^alpha"]
./ret "^alpha" <<EOF
alpha
alpha alpha
not alpha
EOF
echo "[Anchors: alpha$]"
./ret "alpha$" <<EOF
xxx alpha
alpha alpha
alpha not
EOF
echo "[Anchors: ^alpha$]"
./ret "^alpha$" <<EOF
alpha
not alpha
alpha not
EOF
echo "[Anchors: ^(alpha|beta)$]"
./ret "^(alpha|beta)$" <<EOF
alpha
beta
alphabeta
EOF
echo "[Anchors: (^alpha|beta$)]"
./ret "(^alpha|beta$)" <<EOF
alpha trailing
preceeding beta
alpha
beta
 alpha
EOF
echo "[Anchors: ^alpha|^beta$|gamma$]"
./ret "^alpha|^beta$|gamma$" <<EOF
alpha
alpha xxx
beta
gamma
xxx gamma
x alpha
x beta
beta x
gamma x
EOF

# Testing dot (wildcard)

echo "[Single dot: z.z]"
./ret "z.z" <<EOF
zaz
zbz
zz
EOF
echo "[Three dots: z...z]"
./ret "z...z" <<EOF
zaaaz
zbbgz
zz
EOF
echo "[Dot closure: z.*z]"
./ret "z.*z" <<EOF
zaaaz
zlots of characters that aren't z
zz
 z
EOF
echo "[Dot closure followed by alternate: z.*(a|b)]"
./ret "z.*(a|b)" <<EOF
zkkkkkka
zjkjkjkjueyb
zzzzzzzzzzzzzzzzza
zz
EOF
echo "[Dot closure as alternate: z(a.*|b)z]"
./ret "z(a.*|b)z" <<EOF
zakkkkkkz
zbz
zzaaaaaaaaz
zz
EOF
echo "[Dot closure as alternates: z(a.*|bbb|cd.*)z]"
./ret "z(a.*|bbb|cd.*)z" <<EOF
zakksdjksjz
zbbbz
zcdsomthing not z
zz
EOF
echo "[Character class: z[abc]z]"
./ret "z[abc]z" <<EOF
zaz
zbz
zcz
zdz
zz
EOF
echo "[Character class negation: z[^abc]z]"
./ret "z[^abc]z" <<EOF
zdz
zez
zfz
zzz
zaz
zbz
zz
EOF
echo "[Character class range: z[a-y]z]"
./ret "z[a-y]z" <<EOF
zaz
zbz
zcz
zdz
zyz
zzz
z1z
zz
EOF
echo "[Character class ranges: z[a-y0-9]z]"
./ret "z[a-y0-9]z" <<EOF
zaz
zbz
zcz
zdz
zyz
zzz
z1z
z9z
z0z
z5z
z]]]z
zz
EOF
echo "[Character class negation ranges: z[^a-y0-9]z]"
./ret "z[^a-y0-9]z" <<EOF
zzz
zAz
zZz
ZBz
zGz
z]z
z1z
z9z
z0z
z5z
zaz
zyz
zgz
z]]]z
zz
EOF
echo "[Character class meta as regular: z[\^\]]z]"
./ret 'z[\^\]]z' <<EOF
zzz
zaz
zbz
z]z
z^z
EOF
echo "[Character class range with closure: z[a-y]*z]"
./ret "z[a-y]*z" <<EOF
zaz
zbz
zcz
zdz
zyz
zz
zabcdefghijkeyz
zzz
z1z
z]z
EOF
echo "[Character class range with closure at end: zz[a-y]*]"
./ret "zz[a-y]*" <<EOF
zza
zzaaaaaaaaaaaaaaaa
zzabcdefghijuk
xx
EOF
echo "[Character class range with alternates: z(a|[0-9]|b)z]"
./ret "z(a|[0-9]|b)z" <<EOF
zaz
zbz
z0z
z9z
z7z
zzz
xx
EOF
echo "[Character class range with alternates & closure: z(a|[0-9]|b)*z]"
./ret "z(a|[0-9]|b)*z" <<EOF
zaz
zbz
z0ab9z
z9999999999999z
z7aaaaaaaaaaaaaaaz
zzz
xx
EOF
