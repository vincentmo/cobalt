// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/cobalt_web_contents_observer.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/net_errors.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "url/gurl.h"
#endif  // BUILDFLAG(IS_ANDROIDTV)

namespace cobalt {

#if BUILDFLAG(IS_ANDROIDTV)
namespace {
const int kNavigationTimeoutSeconds = 1;
const int kJniErrorTypeConnectionError = 0;
}  // namespace
#endif  // BUILDFLAG(IS_ANDROIDTV)

CobaltWebContentsObserver::CobaltWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
#if BUILDFLAG(IS_ANDROIDTV)
  timeout_timer_ = std::make_unique<base::OneShotTimer>();
#endif  // BUILDFLAG(IS_ANDROIDTV)
}

CobaltWebContentsObserver::~CobaltWebContentsObserver() = default;

#if BUILDFLAG(IS_ANDROIDTV)
void CobaltWebContentsObserver::SetTimerForTestInternal(
    std::unique_ptr<base::OneShotTimer> timer) {
  timeout_timer_ = std::move(timer);
}

void CobaltWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* handle) {
  LOG(INFO) << "DEBUG: DidStartNavigation started";
  if (!handle->IsInPrimaryMainFrame()) {
    LOG(INFO) << "DEBUG: DidStartNavigation: navigation to " << handle->GetURL()
              << " not in primary mainframe, returning";
    return;
  }

  // Start a navigation timer with a timeout callback to raise a
  // network error dialog
  timeout_timer_->Stop();
  LOG(INFO) << "DEBUG: DidStartNavigation is about to start a timer for "
            << kNavigationTimeoutSeconds << " seconds";
  timeout_timer_->Start(
      FROM_HERE, base::Seconds(kNavigationTimeoutSeconds),
      base::BindOnce(&CobaltWebContentsObserver::RaisePlatformError,
                     weak_factory_.GetWeakPtr(), handle->GetURL()));
}

// Opting for WebContentsObserver::DidFinishNavigation() over
// WebContentsObserver::PrimaryPageChanged as the network check can't
// assume HasCommitted() is true. Doing so would not catch network
// errors that are thrown before a navigation commits such as
// net::ERR_CONNECTION_TIMED_OUT and net::ERR_NAME_NOT_RESOLVED.
void CobaltWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  LOG(INFO) << "DEBUG: DidFinishNavigation started <<<";
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    LOG(INFO) << "DEBUG: DidFinishNavigation: navigation to "
              << navigation_handle->GetURL()
              << " not in primary mainframe, returning";
    return;
  } else {
    LOG(INFO) << "DEBUG: DidFinishNavigation: navigation to "
              << navigation_handle->GetURL()
              << " is in primary mainframe, continuing";
  }

  timeout_timer_->Stop();
  const auto net_error_code = navigation_handle->GetNetErrorCode();
  if (net_error_code != net::OK && net_error_code != net::ERR_ABORTED) {
    UMA_HISTOGRAM_BOOLEAN("Cobalt.WebContentsObserver.FailedNavigation", true);
    LOG(INFO)
        << "DEBUG: DidFinishNavigation: Raising platform error with code: "
        << net::ErrorToString(net_error_code);
    RaisePlatformError(navigation_handle->GetURL());
  } else if (net_error_code == net::OK) {
    UMA_HISTOGRAM_BOOLEAN("Cobalt.WebContentsObserver.FailedNavigation", false);
    platform_error_raised_count_ = 0;
  }

  void CobaltWebContentsObserver::DidRedirectNavigation(
      content::NavigationHandle * navigation_handle) {
    LOG(INFO) << "DEBUG: DidRedirectNavigation started";
  }

  void CobaltWebContentsObserver::ReadyToCommitNavigation(
      content::NavigationHandle * navigation_handle) {
    LOG(INFO) << "DEBUG: ReadyToCommitNavigation started";
  }

  void CobaltWebContentsObserver::DidActivatePortal(
      content::WebContents * predecessor_web_contents,
      base::TimeTicks activation_time) {
    LOG(INFO) << "DEBUG: DidActivatePortal started";
  }

  void CobaltWebContentsObserver::DidStartLoading() {
    LOG(INFO) << "DEBUG: DidStartLoading started";
  }

  void CobaltWebContentsObserver::DidStopLoading() {
    LOG(INFO) << "DEBUG: DidStopLoading started";
  }

  void CobaltWebContentsObserver::DOMContentLoaded(content::RenderFrameHost *
                                                   render_frame_host) {
    LOG(INFO) << "DEBUG: DOMContentLoaded started";
  }

  void CobaltWebContentsObserver::DidFinishLoad(
      content::RenderFrameHost * render_frame_host, const GURL& validated_url) {
    LOG(INFO) << "DEBUG: DidFinishLoad started";
  }

  void CobaltWebContentsObserver::DidFailLoad(
      content::RenderFrameHost * render_frame_host, const GURL& validated_url,
      int error_code) {
    LOG(INFO) << "DEBUG: DidFailLoad started";
  }

  void CobaltWebContentsObserver::RaisePlatformError(const GURL& url) {
    LOG(INFO) << "DEBUG: RaisePlatformError started";
    JNIEnv* env = base::android::AttachCurrentThread();
    auto* starboard_bridge =
        starboard::android::shared::StarboardBridge::GetInstance();

    // Don't raise a new platform error if one is already showing
    if (starboard_bridge->IsPlatformErrorShowing(env)) {
      LOG(INFO) << "DEBUG: RaisePlatformError: platform error already showing, "
                   "returning";
      return;
    }
    platform_error_raised_count_++;
    UMA_HISTOGRAM_COUNTS_100("Cobalt.Network.CumulativePlatformErrorRaised",
                             platform_error_raised_count_);
    LOG(INFO) << "DEBUG: calling RaisePlatformError, current url is "
              << url.spec();
    starboard_bridge->RaisePlatformError(env, kJniErrorTypeConnectionError, 0,
                                         url.spec());
  }
#endif  // BUILDFLAG(IS_ANDROIDTV)

}  // namespace cobalt
