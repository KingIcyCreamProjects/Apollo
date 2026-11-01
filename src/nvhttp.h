/**
 * @file src/nvhttp.h
 * @brief Declarations for the nvhttp (GameStream) server.
 */
// macros
#pragma once

// standard includes
#include <string>
#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

// lib includes
#include <boost/property_tree/ptree.hpp>
#include <nlohmann/json.hpp>
#include <Simple-Web-Server/server_https.hpp>

// local includes
#include "crypto.h"
#include "rtsp.h"
#include "thread_safe.h"

using namespace std::chrono_literals;

/**
 * @brief Contains all the functions and variables related to the nvhttp (GameStream) server.
 */
namespace nvhttp {

  using args_t = SimpleWeb::CaseInsensitiveMultimap;
  using cmd_list_t = std::list<crypto::command_entry_t>;

  /**
   * @brief The protocol version.
   * @details The version of the GameStream protocol we are mocking.
   * @note The negative 4th number indicates to Moonlight that this is Sunshine.
   */
  constexpr auto VERSION = "7.1.431.-1";

  /**
   * @brief The GFE version we are replicating.
   */
  constexpr auto GFE_VERSION = "3.23.0.74";

  /**
   * @brief The HTTP port, as a difference from the config port.
   */
  constexpr auto PORT_HTTP = 0;

  /**
   * @brief The HTTPS port, as a difference from the config port.
   */
  constexpr auto PORT_HTTPS = -5;

  constexpr auto OTP_EXPIRE_DURATION = 180s;

  /**
   * @brief How long a pairing session may stay open before it is dropped.
   */
  constexpr auto PAIRING_SESSION_TIMEOUT = 5min;

  /**
   * @brief How long a session waiting for a PIN holds off other clients. Moonlight does not tell
   * the host when its PIN prompt is cancelled, so an older session may be an abandoned one.
   */
  constexpr auto PAIRING_PROMPT_GRACE = 60s;

  /**
   * @brief The most pairing sessions kept at once, since any unauthenticated client can open one.
   */
  constexpr std::size_t MAX_PENDING_PAIRING_SESSIONS = 32;

  /**
   * @brief Start the nvhttp server.
   * @examples
   * nvhttp::start();
   * @examples_end
   */
  void start();

  std::string
  get_arg(const args_t &args, const char *name, const char *default_value = nullptr);

  // Helper function to extract command entries
  cmd_list_t
  extract_command_entries(const nlohmann::json& j, const std::string& key);

  std::shared_ptr<rtsp_stream::launch_session_t>
  make_launch_session(bool host_audio, bool input_only, const args_t &args, const crypto::named_cert_t* named_cert_p);

  /**
   * @brief Setup the nvhttp server.
   * @param pkey
   * @param cert
   */
  void setup(const std::string &pkey, const std::string &cert);

  class SunshineHTTPS: public SimpleWeb::HTTPS {
  public:
    SunshineHTTPS(boost::asio::io_context &io_context, boost::asio::ssl::context &ctx):
        SimpleWeb::HTTPS(io_context, ctx) {
    }

    virtual ~SunshineHTTPS() {
      // Gracefully shutdown the TLS connection
      SimpleWeb::error_code ec;
      shutdown(ec);
    }
  };

  enum class PAIR_PHASE {
    NONE,  ///< Sunshine is not in a pairing phase
    GETSERVERCERT,  ///< Sunshine is in the get server certificate phase
    CLIENTCHALLENGE,  ///< Sunshine is in the client challenge phase
    SERVERCHALLENGERESP,  ///< Sunshine is in the server challenge response phase
    CLIENTPAIRINGSECRET  ///< Sunshine is in the client pairing secret phase
  };

  /**
   * @brief The final result of a pairing session, shared with the pin() call waiting for it.
   */
  struct pairing_completion_t {
    std::condition_variable condition;  ///< Wakes the waiting pin() call.
    std::mutex mutex;  ///< Protects result.
    std::optional<bool> result;  ///< Empty while the handshake is running. The first result wins.
  };

  struct pair_session_t {
    struct {
      std::string uniqueID = {};
      std::string cert = {};
      std::string name = {};
    } client;

    std::unique_ptr<crypto::aes_t> cipher_key = {};
    std::vector<uint8_t> clienthash = {};

    std::string serversecret = {};
    std::string serverchallenge = {};

    struct {
      util::Either<
        std::shared_ptr<typename SimpleWeb::ServerBase<SimpleWeb::HTTP>::Response>,
        std::shared_ptr<typename SimpleWeb::ServerBase<SunshineHTTPS>::Response>>
        response;
      std::string salt = {};
    } async_insert_pin;

    /**
     * @brief used as a security measure to prevent out of order calls
     */
    PAIR_PHASE last_phase = PAIR_PHASE::NONE;

    std::shared_ptr<pairing_completion_t> completion = std::make_shared<pairing_completion_t>();
    std::chrono::steady_clock::time_point expires_at = std::chrono::steady_clock::time_point::max();
  };

  /**
   * @brief Pairing sessions by client uniqueid. Lock map_id_sess_mutex to use them.
   */
  extern std::unordered_map<std::string, pair_session_t> map_id_sess;
  extern std::mutex map_id_sess_mutex;

  /**
   * @brief removes the temporary pairing session
   * @details A pin() call waiting on the session gets a failed result, unless it already has one.
   * The caller holds map_id_sess_mutex.
   * @param sess
   */
  void remove_session(const pair_session_t &sess);

  /**
   * @brief Handle a getservercert request by opening a pairing session.
   * @details The caller holds map_id_sess_mutex.
   * @param args The query arguments of the request.
   * @param uniqID The client uniqueid.
   * @param tree Holds the answer to the request when nullptr is returned.
   * @return The session when it must wait for a PIN, else nullptr.
   */
  pair_session_t *start_pair_session(const args_t &args, const std::string &uniqID, boost::property_tree::ptree &tree);

  /**
   * @brief Pair, phase 1
   *
   * Moonlight will send a salt and client certificate, we'll also need the user provided pin.
   *
   * PIN and SALT will be used to derive a shared AES key that needs to be stored
   * in order to be used to decrypt_symmetric in the next phases.
   *
   * At this stage we only have to send back our public certificate.
   */
  void getservercert(pair_session_t &sess, boost::property_tree::ptree &tree, const std::string &pin);

  /**
   * @brief Pair, phase 2
   *
   * Using the AES key that we generated in phase 1 we have to decrypt the client challenge,
   *
   * We generate a SHA256 hash with the following:
   *  - Decrypted challenge
   *  - Server certificate signature
   *  - Server secret: a randomly generated secret
   *
   * The hash + server_challenge will then be AES encrypted and sent as the `challengeresponse` in the returned XML
   */
  void clientchallenge(pair_session_t &sess, boost::property_tree::ptree &tree, const std::string &challenge);

  /**
   * @brief Pair, phase 3
   *
   * Moonlight will send back a `serverchallengeresp`: an AES encrypted client hash,
   * we have to send back the `pairingsecret`:
   * using our private key we have to sign the certificate_signature + server_secret (generated in phase 2)
   */
  void serverchallengeresp(pair_session_t &sess, boost::property_tree::ptree &tree, const std::string &encrypted_response);

  /**
   * @brief Pair, phase 4 (final)
   *
   * We now have to use everything we exchanged before in order to verify and finally pair the clients
   *
   * We'll check the client_hash obtained at phase 3, it should contain the following:
   *   - The original server_challenge
   *   - The signature of the X509 client_cert
   *   - The unencrypted client_pairing_secret
   * We'll check that SHA256(server_challenge + client_public_cert_signature + client_secret) == client_hash
   *
   * Then using the client certificate public key we should be able to verify that
   * the client secret has been signed by Moonlight
   */
  void clientpairingsecret(pair_session_t &sess, boost::property_tree::ptree &tree, const std::string &client_pairing_secret);

  /**
   * @brief Give the user supplied pin to the one session waiting for it, and wait for the handshake to finish.
   * @details Sessions past PAIRING_PROMPT_GRACE give way to newer ones. A timeout only ends the wait, so the
   * client can still finish the handshake until the session expires.
   * @param pin The user supplied pin.
   * @param name The user supplied name.
   * @return `true` if Moonlight proved the pin by completing the handshake, `false` on a wrong pin,
   * on a timeout of `ping_timeout`, or unless exactly one session is waiting for a pin.
   * @examples
   * bool pin_status = nvhttp::pin("1234", "laptop");
   * @examples_end
   */
  bool pin(std::string pin, std::string name);

  std::string request_otp(const std::string& passphrase, const std::string& deviceName);

  /**
   * @brief Write the paired clients to the state file.
   * @details The caller holds the client state lock, or is a single-threaded test.
   * @return Whether the state file was written.
   */
  bool save_state();

  /**
   * @brief Load the paired clients from the state file.
   * @details The caller holds the client state lock, or is a single-threaded test.
   * A file that can't be parsed leaves the clients in memory as they are.
   */
  void load_state();

  /**
   * @brief Trust a client and save it to the state file.
   * @details A failed save still trusts the client until the host restarts.
   * @param named_cert_p The client to add.
   */
  void add_authorized_client(const crypto::p_named_cert_t &named_cert_p);

  /**
   * @brief Check a client certificate against the paired clients.
   * @param cert The certificate to verify.
   * @param named_cert_out The matching paired client.
   * @return nullptr if the certificate is trusted, otherwise an error string.
   */
  const char *verify_client_cert(crypto::x509_t::element_type *cert, crypto::p_named_cert_t &named_cert_out);

  /**
   * @brief Remove single client.
   * @details The client stops being trusted even when the state file can't be written.
   * @param uuid The UUID of the client to remove.
   * @return Whether a client was removed and the state file was written.
   * @examples
   * nvhttp::unpair_client("4D7BB2DD-5704-A405-B41C-891A022932E1");
   * @examples_end
   */
  bool unpair_client(std::string_view uuid);

  /**
   * @brief Get all paired clients.
   * @return The list of all paired clients.
   * @examples
   * nlohmann::json clients = nvhttp::get_all_clients();
   * @examples_end
   */
  nlohmann::json get_all_clients();

  /**
   * @brief Remove all paired clients.
   * @details The clients stop being trusted even when the state file can't be written.
   * @return Whether the state file was written.
   * @examples
   * nvhttp::erase_all_clients();
   * @examples_end
   */
  bool erase_all_clients();

  /**
   * @brief      Stops a session.
   *
   * @param      session   The session
   * @param[in]  graceful  Whether to stop gracefully
   */
  void stop_session(stream::session_t& session, bool graceful);

  /**
   * @brief      Finds and stop session.
   *
   * @param[in]  uuid      The uuid string
   * @param[in]  graceful  Whether to stop gracefully
   */
  bool find_and_stop_session(const std::string& uuid, bool graceful);

  /**
   * @brief      Update device info associated to the session
   *
   * @param      session  The session
   * @param[in]  name     New name
   * @param[in]  newPerm  New permission
   */
  void update_session_info(stream::session_t& session, const std::string& name, const crypto::PERM newPerm);

  /**
   * @brief      Finds and udpate session information.
   *
   * @param[in]  uuid     The uuid string
   * @param[in]  name     New name
   * @param[in]  newPerm  New permission
   */
  bool find_and_udpate_session_info(const std::string& uuid, const std::string& name, const crypto::PERM newPerm);

  /**
   * @brief      Update device info
   *
   * @param[in]  uuid       The uuid string
   * @param[in]  name       New name
   * @param[in]  do_cmds    The do commands
   * @param[in]  undo_cmds  The undo commands
   * @param[in]  newPerm    New permission
   * @param[in]  enable_legacy_ordering  Enable legacy ordering
   * @param[in]  allow_client_commands  Allow client commands
   * @param[in]  always_use_virtual_display  Always use virtual display
   * 
   * @return     Whether the update is successful
   */
  bool update_device_info(
    const std::string& uuid,
    const std::string& name,
    const std::string& display_mode,
    const cmd_list_t& do_cmds,
    const cmd_list_t& undo_cmds,
    const crypto::PERM newPerm,
    const bool enable_legacy_ordering,
    const bool allow_client_commands,
    const bool always_use_virtual_display
  );
}  // namespace nvhttp
