// Jenkins pipeline file for ventus flow.
//
// Copyright (C) 2020-2023 Terapines Technology (Wuhan) Co., Ltd
// All rights reserved.
//
//

pipeline {
    agent { label "linux-x64" }
    environment {
        GIT_USER_EMAIL = sh(
      script: 'git --no-pager show -s --format="%ae"',
      returnStdout: true
    )
        GIT_USER_NAME = sh(
      script: 'git --no-pager show -s --format="%an"',
      returnStdout: true
    )
        GIT_COMMIT_DETAIL = sh(
      script: 'git --no-pager show -s',
      returnStdout: true
    )
        DATE = sh(
      script: 'date +%Y%m%d',
      returnStdout: true
    ).trim()
        ARCHIVE_FOLDER_NFS = '/share/rd/toolchain/jenkins-built'
        ARCHIVE_FOLDER_SMB = '\\\\share.tpt.com\\rd\\toolchain\\jenkins-built'
    }
    stages {
        stage('The Matrix') {
            matrix {
                agent { label "${AGENT}"
                }
                axes {
                    axis {
                        name 'AGENT'
                        values 'linux-x64'// TODO: 'win64', 'freebsd-x86', 'macos-x86', 'macos-arm'
                    // for now, we only enable linux building process
                    }
                }
                stages {
                    stage('Initial&build') {
                        steps {
                            check_ventus_other_dependency(
                ['zcc-test-suit', 'ocl-icd']
                ['master', 'master'],
                ['git@git.tpt.com:/git/zcc-test-suit.git', 'https://github.com/OCL-dev/ocl-icd.git'])
                            check_ventus_THU_dependency(
                ['llvm-project', 'ventus-gpgpu-isa-simulator', 'pocl']
                ['master', 'master', 'master'])
                            build_ventus_llvm("${AGENT}")
                            build_ventus_spike("${AGENT}")
                        }
                    }
                    stage('Running test') {
                        when {
                            anyOf {
                                triggeredBy 'TimerTrigger'
                                triggeredBy cause: 'UserIdCause'
                            }
                        }
                        steps {
                            echo 'Running spike tests!'
                        }
                    }
                }
            }
        }
    }
    post {
        // Send email on failure
        failure {
            mail(to: "$GIT_USER_EMAIL",
          from: 'jenkins@terapines.com',
          bcc: '',
          replyTo: 'jenkins@terapines.com',
          charset: 'UTF-8',
          mimeType: 'text/html',
          subject: "[CI ERROR] $JOB_NAME, build $BUILD_NUMBER",
          body: "<h3>[$JOB_NAME] Failures in Jenkins test</h3>\
                  <p>Project: $JOB_NAME</p>\
                  <p>Build number: $BUILD_NUMBER</p>\
                  <p>Build URL: $JENKINS_URL/job/zcc</p>\
                  <p>Branch: $GIT_BRANCH</p>\
                  <p>Latest git log:</p>\
                  <p><pre>$GIT_COMMIT_DETAIL</pre><p>")
        }
        always {
            archiveArtifacts artifacts: 'reports/*.json', fingerprint: true, allowEmptyArchive: true
            junit testResults: 'reports/*.xml', allowEmptyResults: true
        }
    }
    // Nightly test runs on every night at 23:00
    triggers {
        cron('0 23 * * *')
    }
}

// check all the repositories from THU
check_ventus_THU_dependency(deps, branches) {
    for (int i = 0; i < deps.size(); i++) {
        sh "mkdir ${deps[i]} || true"
        dir(path: "${deps[i]}") {
            echo "Pulling ${deps[i]}"
            git(url: "ssh://git@github.com:THU-DSP-LAB/${deps[i]}.gitt",
          branch: "${branches[i]}",
          credentialsId: 'git-ssh-pk')
        }
    }
}

// Check other dependency such as zcc-test-suit, ocl-icd
// same as ventus-spike
def check_ventus_other_dependency(deps, branches, sources) {
    for (int i = 0; i < deps.size(); i++) {
        sh "mkdir ${deps[i]} || true"
        dir(path: "${deps[i]}") {
            echo "Pulling ${deps[i]}"
            git(url: sources[i],
          branch: "${branches[i]}",
          credentialsId: 'git-ssh-pk')
        }
    }
}

def build_ventus_llvm(agent) {
    //we only build ventus for linux
    if ("$agent" =~ 'linux-x64') {
        sh "cd $WORKSPACE/llvm-project"
        sh 'bash build-ventus.sh'
    }
}

def build_ventus_spike(agent) {
    //we only build ventus for linux
    if ("$agent" =~ 'linux-x64') {
        sh "cd $WORKSPACE/ventus-gpgpu-isa-simulator"
        sh 'mkdir build || true'
        sh 'cd build'
        sh "export RISCV=\"$WORKSPACE/llvm-project/install\";"
        sh '''
        ../configure --prefix=$RISCV
        make
        make install
      '''
    }
}
