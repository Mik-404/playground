work_dir=$(mktemp -d)
cwd=$(pwd)

function cleanup {
    rm -rf "$work_dir"
    echo "> Deleted temp directory $work_dir"
}

trap cleanup EXIT

python3 -m venv .venv
source .venv/bin/activate
pip install wheel
pip install "setuptools<81"

echo '> Created venv'

git clone --filter=blob:none https://gitlab.com/qemu-project/qemu.git $work_dir/qemu
cd $work_dir/qemu
git checkout stable-8.2
pip install --no-build-isolation $work_dir/qemu/python

echo '> Installed QEMU python lib'

cd $cwd
pip install -r requirements.txt

echo '> Installed other python deps'

echo
echo

echo 'Done. Run following command to activate python venv:'
echo '  source .venv/bin/activate'
