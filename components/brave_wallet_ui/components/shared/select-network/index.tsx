import * as React from 'react'
import SelectNetworkItem from '../select-network-item'
import { BraveWallet } from '../../../constants/types'

export interface Props {
  networks: BraveWallet.NetworkInfo[]
  selectedNetwork: BraveWallet.NetworkInfo
  onSelectNetwork: (network: BraveWallet.NetworkInfo) => () => void
}

function SelectNetwork (props: Props) {
  const { networks, onSelectNetwork, selectedNetwork } = props
  return (
    <>
      {networks.map((network) =>
        <SelectNetworkItem
          selectedNetwork={selectedNetwork}
          key={network.chainId}
          network={network}
          onSelectNetwork={onSelectNetwork(network)}
        />
      )}
    </>
  )
}

export default SelectNetwork
