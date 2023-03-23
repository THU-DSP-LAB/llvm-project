// Jenkins pipeline file for ventus flow.
//
// Copyright (C) 2020-2023 Terapines Technology (Wuhan) Co., Ltd
// All rights reserved.
//
//

pipeline {
    agent any
    environment {
    GIT_USER_EMAIL = sh (
      script: 'git --no-pager show -s --format="%ae"',
      returnStdout: true
    )
    GIT_USER_NAME = sh (
      script: 'git --no-pager show -s --format="%an"',
      returnStdout: true
    )
    GIT_COMMIT_DETAIL = sh (
      script: 'git --no-pager show -s',
      returnStdout: true
    )
    DATE = sh (
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
              values 'linux-x64', 'win64' // TODO: 'freebsd-x86', 'macos-x86', 'macos-arm'
          }
        }
        stages {
          stage('Build dependency') {
              when {
                anyOf {
                  triggeredBy 'TimerTrigger'
                  triggeredBy cause: 'UserIdCause'
                }
              }
              steps {
                build_dependency()
              }
            }
          stage('Build ventus') {
              when {
                anyOf {
                  triggeredBy 'TimerTrigger'
                  triggeredBy cause: 'UserIdCause'
                }
              }
              steps {
                // build ventus,this need to be modified later
                  sh "bash /work-test/ventus-git/build-ventus.sh --build llvm-ventus"
                  echo "$WORKSPACE"
              }
            }
          stage('Build spike') {
              when {
                anyOf {
                  triggeredBy 'TimerTrigger'
                  triggeredBy cause: 'UserIdCause'
                }
              }
              steps {
                  echo "Build spike from THU"
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
                  echo "Running spike tests!"
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
}

def build_dependency() {
    echo "Pulling all the needed repositories"
}

def build_ventus_test_suite(agent) {
  //we only build ventus for linux
  if("$agent" =~ "linux-x64") {
    echo "This is ventus!"
  }
}
