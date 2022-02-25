/* Copyright 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_DE_AMP_BROWSER_DE_AMP_URL_LOADER_H_
#define BRAVE_COMPONENTS_DE_AMP_BROWSER_DE_AMP_URL_LOADER_H_

#include <string>
#include <tuple>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "brave/components/de_amp/browser/de_amp_service.h"
#include "brave/components/sniffer/sniffer_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/mojom/early_hints.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace de_amp {

class DeAmpThrottle;

class DeAmpURLLoader : public sniffer::SnifferURLLoader {
 public:
  ~DeAmpURLLoader() override;

  // mojo::PendingRemote<network::mojom::URLLoader> controls the lifetime of the
  // loader.
  static std::tuple<mojo::PendingRemote<network::mojom::URLLoader>,
                    mojo::PendingReceiver<network::mojom::URLLoaderClient>,
                    DeAmpURLLoader*>
  CreateLoader(base::WeakPtr<sniffer::SnifferThrottle> throttle,
               const GURL& response_url,
               scoped_refptr<base::SequencedTaskRunner> task_runner,
               DeAmpService* service,
               content::WebContents* contents);

 private:
  DeAmpURLLoader(base::WeakPtr<sniffer::SnifferThrottle> throttle,
                 const GURL& response_url,
                 mojo::PendingRemote<network::mojom::URLLoaderClient>
                     destination_url_loader_client,
                 scoped_refptr<base::SequencedTaskRunner> task_runner,
                 DeAmpService* service,
                 content::WebContents* contents);

  void OnBodyReadable(MojoResult) override;
  void OnBodyWritable(MojoResult) override;

  void CompleteSending() override;
  void ForwardBodyToClient();

  content::WebContents* contents_;
  DeAmpService* de_amp_service;
};

}  // namespace de_amp

#endif  // BRAVE_COMPONENTS_DE_AMP_BROWSER_DE_AMP_URL_LOADER_H_
