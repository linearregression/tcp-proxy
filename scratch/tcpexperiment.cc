/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Default Network Topology
//
//    LAN 10.0.X.0
//  ================
//  |    |    |    |    10.1.Y.0          10.2.0.0
// n0   n1   n2   n6 -------------- n7 -------------- n8   n3   n4   n5
//                   point-to-point    point-to-point  |    |    |    |
//                                                     ================
//                                                       LAN 10.3.X.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpExperiment");

/* * * * * * * * * * * * * START OF TcpProxy CLASS * * * * * * * * * * * * */

class TcpProxy : public Application
{
public:
  static TypeId GetTypeId (void);
  
  TcpProxy ();
  
  virtual ~TcpProxy ();
  
  virtual void AddPair (const Address srcip, uint16_t srcport, const Address dstip, uint16_t dstport);
  virtual void SetPort (uint16_t port);
  
protected:
  virtual void DoDispose (void);
  virtual bool HandleRequest (Ptr<Socket> socket, const Address &address);
  virtual void HandleConnectionCreated (Ptr<Socket> socket, const Address &address);
  virtual void HandleRecv (Ptr<Socket> socket);
  virtual void HandleSend (Ptr<Socket> socket, uint32_t bytesSent);
  virtual void Forward (Ptr<Socket> src, Ptr<Socket> dst);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  
  uint16_t m_port;                      //!< Listening port
  Ptr<Socket> m_socket;                 //!< Listener socket
  std::vector<Ptr<Socket> > m_iSockets; //!< input sockets
  std::vector<Ptr<Socket> > m_oSockets; //!< output sockets
  std::map<Ptr<Socket>, int> m_map;     //!< maps sockets to their indices
  std::map<Address, Address> m_pair;    //!< maps addresses to their pair
};

TypeId
TcpProxy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpProxy")
    .SetParent<Application> ()
    .AddConstructor<TcpProxy> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpProxy::m_port),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

TcpProxy::TcpProxy ()
{
  NS_LOG_FUNCTION (this);
}

TcpProxy::~TcpProxy()
{
  NS_LOG_FUNCTION (this);
}

void
TcpProxy::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
TcpProxy::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
	  
	  m_socket->Bind (InetSocketAddress(m_port));
      
	  m_socket->SetAcceptCallback (MakeCallback (&TcpProxy::HandleRequest, this),
								   MakeCallback (&TcpProxy::HandleConnectionCreated, this));
      m_socket->Listen ();
    }
  
  m_iSockets.reserve(8);
  m_oSockets.reserve(8);
}

void 
TcpProxy::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetAcceptCallback (MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
								   MakeNullCallback<void, Ptr<Socket>, const Address &> ());
      m_socket = 0;
    }
}

bool
TcpProxy::HandleRequest (Ptr<Socket> socket, const Address &address)
{
  NS_LOG_FUNCTION (this << socket << address);
  
  return m_pair.find (InetSocketAddress::ConvertFrom(address).GetIpv4 ()) != m_pair.end();
}

void
TcpProxy::HandleConnectionCreated (Ptr<Socket> socket, const Address &address)
{
  NS_LOG_FUNCTION (this << socket << address);
  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  Ptr<Socket> oSocket = Socket::CreateSocket (GetNode (), tid);
  Ipv4Address srcip = InetSocketAddress::ConvertFrom (address).GetIpv4 ();
  InetSocketAddress disa = InetSocketAddress::ConvertFrom (m_pair[srcip]);
  
  if (oSocket->Connect (disa) == 0)
	{
	  // If connection is successful, add sockets to the map
	  m_map[oSocket] = m_oSockets.size ();
	  m_map[socket] = m_iSockets.size ();
  
	  // Remember both sockets to retrieve them later using the map
	  m_oSockets.push_back (oSocket);
	  m_iSockets.push_back (socket);
	  
	  // Set callbacks for package reception
	  oSocket->SetRecvCallback (MakeCallback (&TcpProxy::HandleRecv, this));
	  socket->SetRecvCallback (MakeCallback (&TcpProxy::HandleRecv, this));
	  
	  // Set callbacks for new available space to transmit
	  oSocket->SetSendCallback (MakeCallback (&TcpProxy::HandleSend, this));
	  socket->SetSendCallback (MakeCallback (&TcpProxy::HandleSend, this));
	  
	  return;
	}
  else
	{
	  NS_LOG_WARN ("Connection failed");
	}
}

void
TcpProxy::HandleRecv (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  
  Ptr<Socket> iSocket;
  Ptr<Socket> oSocket;
  
  NS_ABORT_MSG_UNLESS (m_map.find (socket) != m_map.end (), "Unmapped socket");
  
  iSocket = m_iSockets[m_map[socket]];
  oSocket = m_oSockets[m_map[socket]];
  
  if (oSocket == socket)
	{
	  oSocket = iSocket;
	  iSocket = socket;
	}
  
  NS_LOG_LOGIC ("iSocket" << iSocket);
  NS_LOG_LOGIC ("oSocket" << oSocket);
  
  Forward (iSocket, oSocket);
}

void
TcpProxy::HandleSend (Ptr<Socket> socket, uint32_t bytesSent)
{
  NS_LOG_FUNCTION (this << socket << bytesSent);
  
  Ptr<Socket> iSocket;
  Ptr<Socket> oSocket;
  
  NS_ABORT_MSG_UNLESS (m_map.find (socket) != m_map.end (), "Unmapped socket");
  
  iSocket = m_iSockets[m_map[socket]];
  oSocket = m_oSockets[m_map[socket]];
  
  if (iSocket == socket)
	{
	  iSocket = oSocket;
	  oSocket = socket;
	}
  
  NS_LOG_LOGIC ("iSocket" << iSocket);
  NS_LOG_LOGIC ("oSocket" << oSocket);
  
  Forward (iSocket, oSocket);
}

void
TcpProxy::Forward (Ptr<Socket> src, Ptr<Socket> dst)
{
  NS_LOG_FUNCTION (this << src << dst);
  
  Ptr<Packet> packet;
  while (true)
	{
	  if (src->GetRxAvailable () <= 0)
		{
		  NS_LOG_INFO ("No data to forward. Returning...");
		  break;
		}
	  if (dst->GetTxAvailable () <= 0)
		{
		  NS_LOG_INFO ("No space in send buffer. Returning...");
		  break;
		}
	  packet = src->Recv (dst->GetTxAvailable (), 0u);
	  uint32_t size = packet->GetSize ();
	  uint32_t real = dst->Send (packet);
	  
	  if (size == real)
		{
		  NS_LOG_INFO ("Packet forwarded (" << size << " bytes)");
		}
	  else
		{
		  NS_LOG_WARN ("Packet was not forwarded correctly (" << size << " bytes expected, sent " << real << ")");
		}
	}
}

void
TcpProxy::AddPair (const Address srcip, uint16_t srcport, const Address dstip, uint16_t dstport)
{
  NS_LOG_FUNCTION (this << srcip << srcport << dstip << dstport);
  InetSocketAddress sisa = InetSocketAddress (Ipv4Address::ConvertFrom(dstip), dstport);
  InetSocketAddress disa = InetSocketAddress (Ipv4Address::ConvertFrom(srcip), srcport);
  m_pair[srcip] = Address(sisa);
  m_pair[dstip] = Address(disa);
}

void
TcpProxy::SetPort (uint16_t port)
{
  NS_LOG_FUNCTION (this << port);
  m_port = port;
}

/* * * * * * * * * * * * * END OF TcpProxy CLASS * * * * * * * * * * * * */

uint32_t cWnd0, cWnd1, cWnd2;

static void
CwndCng0 (uint32_t oldCwnd, uint32_t newCwnd)
{
  printf ("%.3f\t%d\t%d\t%d\n", Simulator::Now ().GetSeconds (), cWnd0, cWnd1, cWnd2);
  cWnd0 = newCwnd;
  printf ("%.3f\t%d\t%d\t%d\n", Simulator::Now ().GetSeconds (), cWnd0, cWnd1, cWnd2);
}

static void
CwndCng1 (uint32_t oldCwnd, uint32_t newCwnd)
{
  printf ("%.3f\t%d\t%d\t%d\n", Simulator::Now ().GetSeconds (), cWnd0, cWnd1, cWnd2);
  cWnd1 = newCwnd;
  printf ("%.3f\t%d\t%d\t%d\n", Simulator::Now ().GetSeconds (), cWnd0, cWnd1, cWnd2);
}

static void
CwndCng2 (uint32_t oldCwnd, uint32_t newCwnd)
{
  printf ("%.3f\t%d\t%d\t%d\n", Simulator::Now ().GetSeconds (), cWnd0, cWnd1, cWnd2);
  cWnd2 = newCwnd;
  printf ("%.3f\t%d\t%d\t%d\n", Simulator::Now ().GetSeconds (), cWnd0, cWnd1, cWnd2);
}

static void
CwndCng3 (uint32_t oldCwnd, uint32_t newCwnd)
{
  ;
}

static void
CwndCng4 (uint32_t oldCwnd, uint32_t newCwnd)
{
  ;
}

typedef void (*FunPtr) (uint32_t ol, uint32_t nw);
FunPtr CwndChange[] = {CwndCng0, CwndCng1, CwndCng2, CwndCng3, CwndCng4};

int
main (int argc, char *argv[])
{
  float start = 0.0;
  float stop = -1.0;
  float dt = 1.0;
  uint32_t data = 1073741824;
  bool proxy = false;
  char delay[] = "60ms";
  char protocol[] = "NewReno";
  
  uint32_t nSubnets = 1;
  uint32_t szSubnet = 3;
  uint16_t sPort = 9; // well-known discard protocol port number
  uint16_t proxyPort = 1024;
  
  CommandLine cmd;
  cmd.AddValue("duration", "Simulation length, in seconds", stop);
  cmd.AddValue("nSubnets", "Number of client subnets", nSubnets);
  cmd.AddValue("szSubnet", "Number of clients per subnet", szSubnet);
  cmd.AddValue("data", "Max data to send for each client", data);
  cmd.AddValue("delay", "Delay on (two) central links, RTT will be 4*delay", delay);
  cmd.AddValue("protocol", "Congestion control protocol to use", protocol);
  cmd.AddValue("proxy", "Enable proxy", proxy);
  cmd.Parse (argc, argv);
  
  if (nSubnets < 1 || szSubnet < 1)
	{
	  std::cout << "Number of clients must be more than zero." << std::endl;
	  exit (1);
	}

  std::stringstream pTypeId;
  pTypeId << "ns3::Tcp" << protocol;

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
					  TypeIdValue (TypeId::LookupByName (pTypeId.str ())));
  
  Time::SetResolution (Time::NS);

  NS_LOG_INFO ("Creating topology...");
  NS_LOG_LOGIC ("Creating nodes...");

  std::vector<NodeContainer> subnets(nSubnets);
  NodeContainer servers;
  NodeContainer routers;
  
  //Iterate over subnets and create routers and clients
  for (uint32_t i=0; i < nSubnets; ++i)
	{
	  // One router per subnet
	  routers.Create (1);
	  // Create subnet clients
	  subnets[i].Create (szSubnet);
	  // One server per client
	  servers.Create (szSubnet);
	}
  // Create middle and server routers
  routers.Create (2);
  
  NS_LOG_LOGIC ("Done creating " << (nSubnets * (2 * szSubnet + 1) + 2)
		  << " nodes.");
  
  NS_LOG_LOGIC ("Creating channels...");
  
  std::vector<NetDeviceContainer> deviceContainers (nSubnets * (2 * szSubnet + 1) + 1);
  
  PointToPointHelper clinker;
  // Set attributes
  clinker.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  clinker.SetChannelAttribute("Delay", StringValue("0.1ms"));
  
  for (uint32_t i=0; i < nSubnets; ++i)
	{
	  // Create connections over each client subnet
	  for (uint32_t j=0; j < szSubnet; ++j)
		{
		  NodeContainer cnodes;
		  cnodes.Add (routers.Get(i));
		  // Connect each node to its router
		  cnodes.Add (subnets[i].Get(j));
		  
		  deviceContainers[szSubnet * i + j] = (clinker.Install (cnodes));
		  clinker.EnablePcap ("tcpexp", deviceContainers[szSubnet * i + j].Get (1), true);
		}
	}
  
  // Connect all client routers to the middle router
  for (uint32_t i=0; i < nSubnets; ++i)
	{
	  NodeContainer nodes;
	  nodes.Add (routers.Get(i));
	  nodes.Add (routers.Get(nSubnets));
	  
	  PointToPointHelper linker;
	  // Set attributes
	  linker.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
	  linker.SetChannelAttribute("Delay", StringValue(delay));
	  
	  deviceContainers[szSubnet * nSubnets + i] = linker.Install (nodes);

	  //linker.EnablePcap ("tcpexp", deviceContainers[nSubnets + i].Get (0), true);
	}
  
  // Connect middle router to server router
  NodeContainer nodes;
  nodes.Add (routers.Get(nSubnets));
  nodes.Add (routers.Get(nSubnets+1));
  
  PointToPointHelper linker;
  // Set attributes
  linker.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  linker.SetChannelAttribute("Delay", StringValue(delay));

  deviceContainers[(szSubnet + 1) * nSubnets] = linker.Install (nodes);
  //linker.EnablePcap ("tcpexp", deviceContainers[2 * nSubnets].Get (0), true);
  
  // Finally, connect server subnet
  PointToPointHelper slinker;
  // Set attributes
  slinker.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  slinker.SetChannelAttribute("Delay", StringValue("0.1ms"));

  // Add each server to their subnet
  for (uint32_t i = 0; i < nSubnets * szSubnet; ++i)
	{
	  NodeContainer snodes;
	  snodes.Add (servers.Get(i));
	  snodes.Add (routers.Get(nSubnets+1));
	  deviceContainers[(szSubnet + 1) * nSubnets + 1 + i] = slinker.Install (snodes);
	  //slinker.EnablePcap ("tcpexp", deviceContainers[2 * nSubnets + 1].Get (0), true);
	}

  NS_LOG_LOGIC ("Done creating channels.");

  NS_LOG_LOGIC ("Installing internet stack...");
  
  InternetStackHelper stack;
  for (uint32_t i=0; i < nSubnets; ++i)
	{
	  stack.Install (subnets[i]);
	}
  stack.Install (routers);
  stack.Install (servers);

  NS_LOG_LOGIC ("Done installing internet stack.");
  
  NS_LOG_LOGIC ("Setting addresses...");
  
  Ipv4AddressHelper addressHlpr;
  
  // Iface containers to recall assigned IPs later during app installation
  std::vector<Ipv4InterfaceContainer> cIpIfaces(nSubnets * szSubnet);
  std::vector<Ipv4InterfaceContainer> pIpIfaces(nSubnets);
  std::vector<Ipv4InterfaceContainer> sIpIfaces(nSubnets * szSubnet);
  
  for (uint32_t i=0; i < nSubnets * szSubnet; i++)
	{
	  std::ostringstream sNetIp;
	  sNetIp << "10.0." << i << ".0";
	  addressHlpr.SetBase(sNetIp.str().c_str(), "255.255.255.0");
	  cIpIfaces[i] = addressHlpr.Assign(deviceContainers[i]);
	}
  
  for (uint32_t i=0; i < nSubnets; i++)
	{
	  std::ostringstream sNetIp;
	  sNetIp << "10.1." << i << ".0";
	  addressHlpr.SetBase(sNetIp.str().c_str(), "255.255.255.0");
	  pIpIfaces[i] = addressHlpr.Assign(deviceContainers[nSubnets * szSubnet + i]);
	}
  
  addressHlpr.SetBase("10.2.0.0", "255.255.255.0");
  addressHlpr.Assign(deviceContainers[nSubnets * (szSubnet + 1)]);
  
  for (uint32_t i=0; i < nSubnets * szSubnet; i++)
	{
	  std::ostringstream sNetIp;
	  sNetIp << "10.3." << i << ".0";
	  addressHlpr.SetBase(sNetIp.str().c_str(), "255.255.255.0");
	  sIpIfaces[i] = addressHlpr.Assign(deviceContainers[(szSubnet + 1) * nSubnets + 1 + i]);
	}

  NS_LOG_LOGIC ("Done setting addresses.");

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  
  NS_LOG_INFO ("Done creating topology.");
  
  int x = 0;

  std::vector<Ptr<TcpSendApplication> > senders (nSubnets * szSubnet);
  std::vector<Ptr<PacketSink> > sinks (nSubnets * szSubnet);
  
  Ptr<TcpProxy> proxyapp = CreateObject<TcpProxy> ();
  
  proxyapp->SetPort (proxyPort);
  proxyapp->SetStartTime (Seconds (start));
  if (stop > 0)
	{
	  proxyapp->SetStopTime (Seconds (stop + dt * nSubnets * nSubnets * szSubnet * szSubnet));
	}
  
  routers.Get (nSubnets)->AddApplication (proxyapp);
  
  for (uint32_t i=0; i < nSubnets; ++i)
	{
	  for (uint32_t j=0; j < szSubnet; ++j, ++x)
		{
		  Ipv4Address caddr = cIpIfaces[i*szSubnet+j].GetAddress (1);
		  Ipv4Address saddr = sIpIfaces[i*szSubnet+j].GetAddress (0);
		  Ipv4Address paddr = saddr;
		  uint16_t pPort = sPort;
		  
		  if (proxy && j == 2u)
			{
			  paddr = pIpIfaces[i].GetAddress (1);
			  proxyapp->AddPair (caddr, 0, saddr, sPort);
			  pPort = proxyPort;
			}
		  
		  TcpSendHelper tsh("ns3::TcpSocketFactory", InetSocketAddress(paddr, pPort));
		  tsh.SetAttribute ("MaxBytes", UintegerValue (data));
		  ApplicationContainer sender = tsh.Install (subnets[i].Get (j));
		  sender.Start (Seconds (start + dt * x * x));
		  if (stop > 0)
			{
			  sender.Stop (Seconds (stop + dt * x * x));
			}
		  
		  Ptr<Socket> skt = DynamicCast<TcpSendApplication> (sender.Get (0))->GetSocket ();
		  skt->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (CwndChange[i * szSubnet + j]));
		  
		  PacketSinkHelper psh("ns3::TcpSocketFactory",
								InetSocketAddress(saddr, sPort));
		  ApplicationContainer sink = psh.Install(servers.Get(i * szSubnet + j));
		  sink.Start (Seconds (start));
		  if (stop > 0)
			{
			  sink.Stop (Seconds (stop + dt * x * x));
			}
		  senders[i * szSubnet + j] = DynamicCast<TcpSendApplication>(sender.Get(0));
		  sinks[i * szSubnet + j] = DynamicCast<PacketSink>(sink.Get(0));
		}
	}
  if (stop > 0)
	{
	  Simulator::Stop (Seconds (stop + dt * x * x));
	}
  NS_LOG_INFO ("Starting simulation...");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation completed.");
  x = 0;
  for (uint32_t i = 0; i < nSubnets; ++i)
	{
	  for (uint32_t j = 0; j < szSubnet; ++j, ++x)
		{
		  uint32_t rx = sinks[i*szSubnet+j]->GetTotalRx();
		  Time t_start = Seconds (start + dt * x * x);
		  Time t_end = std::max(senders[i*szSubnet+j]->GetLastAckTime(), sinks[i*szSubnet+j]->GetLastPktTime());
		  double time = (t_end - t_start).GetSeconds ();
		  NS_LOG_UNCOND ("# Total Bytes Received on server "
						  << (szSubnet * i + j) << ": " << rx);
		  std::cout << "# Total bytes Received on server "
						  << (szSubnet * i + j) << ": " << rx << std::endl;
		  std::cout << "# Total time for connection "
						  << (szSubnet * i + j) << ": " << time << " s." << std::endl;
		  std::cout << "# Throughput on connection "
						  << (szSubnet * i + j) << ": " << rx/time/128.0 << " Kbps." << std::endl;
		  std::cout << "#" << std::endl;
		}
	}
  return 0;
}
