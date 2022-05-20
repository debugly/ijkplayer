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
# copy local repo from $1 to $2，just contain lastest commit. 

if [[ -z $1 || -z $2 ]]; then
    echo "invalid argvs for $0"
    exit -1
fi

function main() {
    local src_repo=$1
    local dest_repo=$2
    
    cd $src_repo
    local full_src_repo_path="file://"$(PWD)
    cd - >/dev/null

    if [[ -d $dest_repo ]]; then
        rm -rf "$dest_repo"
    fi

    # clone local repo.
    git clone "$full_src_repo_path" $dest_repo --depth=1
}

main $*