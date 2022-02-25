import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './federated_internals.mojom-webui.js';

export class FederatedInternalsBrowserProxy {
  private handler: PageHandlerRemote;
  private callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getAdStoreInfo() {
    return this.handler.getAdStoreInfo();
  }

  static getInstance(): FederatedInternalsBrowserProxy {
    return instance || (instance = new FederatedInternalsBrowserProxy());
  }

  getCallbackRouter(): PageCallbackRouter {
    return this.callbackRouter;
  }
}

let instance: FederatedInternalsBrowserProxy|null = null;