#! /usr/bin/env bash
#
# Copyright (C) 2021 Matt Reach<qianlongxu@gmail.com>

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

set -e

TOOLS=$(dirname "$0")
source ${TOOLS}/env_assert.sh

echo "===check env begin==="
echo "argv:$*"
env_assert "GIT_UPSTREAM"
env_assert "GIT_LOCAL_REPO"
env_assert "GIT_COMMIT"
env_assert "REPO_DIR"
echo "===check env end==="

iOS_ARCHS="x86_64 arm64"
macOS_ARCHS="x86_64 arm64"

function apply_patches()
{
    local patch_dir="${TOOLS}/../extra/patches/$REPO_DIR"
    if [[ -d "$patch_dir" ]];then
        echo "Applying patches to $REPO_DIR"
        rm -rf .git/rebase-apply
        git am $patch_dir/*.patch
        if [[ $? -ne 0 ]]; then
            echo 'Apply patches failed!'
            exit 1
        fi
    fi
}

function pull_common() {
    echo "== pull $REPO_DIR base =="
    if [[ -d "$GIT_LOCAL_REPO" ]];then
        git -C $GIT_LOCAL_REPO reset --hard
    fi
    sh $TOOLS/pull-repo-base.sh $GIT_UPSTREAM $GIT_LOCAL_REPO
}

function pull_fork() {
    local dir="build/src/$1/$REPO_DIR-$2"
    echo "== pull $REPO_DIR fork to $dir =="
    if [[ -d "$dir" ]];then
        git -C "$dir" reset --hard
    fi
    sh $TOOLS/pull-repo-ref.sh $GIT_UPSTREAM $dir $GIT_LOCAL_REPO
    cd $dir
    git checkout ${GIT_COMMIT} -B localBranch
    echo "last commit:"$(git log -1 --pretty=format:"[%h] %s:%ce %cd")
    apply_patches
    cd - > /dev/null
}

function usage() {
    echo "usage:"
    echo "$0 ios|macos|all [arm64|x86_64]"
}

function main() {
    case "$1" in
        iOS|ios)
            pull_common
            found=0
            for arch in $iOS_ARCHS
            do
                if [[ "$2" == "$arch" || "x$2" == "x" ]];then
                    found=1
                    pull_fork 'ios' $arch
                fi
            done

            if [[ found -eq 0 ]];then
                echo "unknown arch:$2 for $1"
            fi
        ;;

        macOS|macos)
            
            pull_common
            found=0
            for arch in $macOS_ARCHS
            do
                if [[ "$2" == "$arch" || "x$2" == "x" ]];then
                    found=1
                    pull_fork 'macos' $arch
                fi
            done

            if [[ found -eq 0 ]];then
                echo "unknown arch:$2 for $1"
            fi
        ;;

        all)
            pull_common
            for arch in $iOS_ARCHS
            do
                pull_fork 'ios' $arch
            done

            for arch in $macOS_ARCHS
            do
                pull_fork 'macos' $arch
            done
        ;;

        *)
            usage
            exit 1
        ;;
    esac
}

main $*
